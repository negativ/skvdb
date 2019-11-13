#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

#include <boost/lockfree/queue.hpp>

#include "util/SpinLock.hpp"
#include "util/Status.hpp"

namespace skv::util {

/**
 * @brief Thread pool basic implementation.
 */
template <typename Backoff = FixedStepSleepBackoff<256, 50>> // try for 256 steps, 50ms sleep
class ThreadPool final {
public:
    ThreadPool(std::size_t nThreads = std::thread::hardware_concurrency()):
        done_{false},
        joiner_{threadPool_}
    {
        if (nThreads == 0)
            nThreads = std::thread::hardware_concurrency();

        try {
            std::generate_n(std::back_inserter(threadPool_),
                            nThreads,
                            [this] { return std::thread(&ThreadPool::routine, this); });
        }
        catch (...) {
            markDone();
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    ~ThreadPool() noexcept {
        markDone();
    }

    template <typename F, typename ... Args, std::enable_if_t<std::is_invocable_v<F, Args...>, int> = 0>
    [[nodiscard]] auto schedule(F&& f, Args&& ... args) {
        if (done())
            throw std::runtime_error{"Thread pool has stopped his work"};

        using result= std::invoke_result_t<F, Args...>;
        using packaged_task = std::packaged_task<result()>;
        using packaged_task_ptr = std::shared_ptr<packaged_task>;

        packaged_task_ptr task = std::make_shared<packaged_task>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        auto future = task->get_future();
        auto cw = createCallWrapper([task{std::move(task)}] { (*task)(); });

        while (!taskQueue_.push(cw));

        return future;
    }

    [[nodiscard]] bool done() const noexcept {
        return done_.load(std::memory_order_acquire);
    }

    void throttle() {
        if (!done()) {
            auto [status, task] = nextTask();

            if (status.isOk())
                task();
            else
                std::this_thread::yield();
        }
    }

private:
    using container_type = std::vector<std::thread>;

    class Joiner final {
    public:
        Joiner(container_type& threads):
            threads_{std::ref(threads)}
        {}

        ~Joiner() {
            std::for_each(std::begin(threads_.get()), std::end(threads_.get()),
                          [](auto& t) { if (t.joinable()) t.join(); });
        }

        Joiner(const Joiner&) = delete;
        Joiner& operator=(const Joiner&) = delete;

        Joiner(Joiner&&) = delete;
        Joiner& operator=(Joiner&&) = delete;

    private:
        std::reference_wrapper<container_type> threads_;
    };

    class CallWrapper {
    public:
        using function = std::function<void()>;

        CallWrapper() = default;
        CallWrapper(function&& f):
            call{std::move(f)}
        {}


        ~CallWrapper() noexcept = default;

        void operator()() {
            call();
        }

    private:
        function call{[]() {}};
    };

    void markDone() {
        done_.store(true, std::memory_order_release);
    }

    void routine() {
        std::size_t step = 0;

        while (!done() || hasTasks()) {
            ++step;

            auto [status, task] = nextTask();

            if (!status.isOk()) {
                Backoff::backoff(step);
            }
            else {
                task();

                step = 0;
            }
        }
    }

    [[nodiscard]] bool hasTasks() const noexcept {
        return !taskQueue_.empty();
    }

    [[nodiscard]] std::tuple<Status, CallWrapper> nextTask() {
        CallWrapper* cw;

        if (taskQueue_.pop(cw)) {
            CallWrapper ret = std::move(*cw);

            destroyCallWrapper(cw);

            return {Status::Ok(), ret};
        }

        return {Status::InvalidOperation("Task queue empty"), CallWrapper{}};
    }

    template<typename F>
    [[nodiscard]] CallWrapper* createCallWrapper(F&& f) {
        return new CallWrapper{std::forward<F>(f)};
    }

    void destroyCallWrapper(CallWrapper* cw) {
        delete cw;
    }

    std::atomic<bool> done_;
    std::vector<std::thread> threadPool_;
    Joiner joiner_;
    boost::lockfree::queue<CallWrapper*,
                           boost::lockfree::fixed_sized<true>,
                           boost::lockfree::capacity<2048>> taskQueue_;
};

}
