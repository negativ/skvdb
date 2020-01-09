#pragma once

#include <algorithm>
#include <atomic>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <vector>

#include <boost/lockfree/queue.hpp>

#include "util/Status.hpp"

namespace skv::util {

template <std::size_t Steps = 10000, std::size_t SleepMs = 50>
struct FixedStepSleepBackoff {
    static void backoff(std::size_t step) {
        if (step % Steps == 0)
            std::this_thread::sleep_for(std::chrono::milliseconds{SleepMs});
        else
            std::this_thread::yield(); // reduces concurency for taskQueue_
    }
};

/**
 * @brief Thread pool basic implementation.
 */
template <typename Backoff = FixedStepSleepBackoff<256, 50>> // try for 256 steps, 50ms sleep
class ThreadPool final {
public:
    ThreadPool(std::size_t nThreads = std::thread::hardware_concurrency()):
        done_{false}
    {
        if (nThreads == 0)
            nThreads = std::thread::hardware_concurrency();

        try {
            std::generate_n(std::back_inserter(threadPool_),
                            nThreads,
                            [this] { return std::thread(&ThreadPool::routine, this); });
        }
        catch (...) {
            stop();

            throw;
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    ~ThreadPool() noexcept {
        stop();
    }

    template <typename F, typename ... Args, std::enable_if_t<std::is_invocable_v<F, Args...>, int> = 0>
    [[nodiscard]] auto schedule(F&& f, Args&& ... args) {
        using result = std::invoke_result_t<F, Args...>;
        using packaged_task = std::packaged_task<result()>;

        auto task = std::make_shared<packaged_task>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        auto future = task->get_future();
        auto cw = createFunction([task{std::move(task)}] { (*task)(); });

        while (!taskQueue_.push(cw));

        return future;
    }

    [[nodiscard]] bool done() const noexcept {
        return done_.load(std::memory_order_acquire);
    }

    void throttle() {
        auto [status, task] = nextTask();

        if (status.isOk() && task) {
            invokeFunction(task);
            destroyFunction(task);
        }
        else
            std::this_thread::yield();
    }

private:
    using container_type = std::vector<std::thread>;
    using function = std::function<void()>;
    using function_ptr = function*;

    void stop() {
        markDone();

        for (auto &t : threadPool_) {
            if (t.joinable())
                t.join();
        }
    }

    void markDone() {
        done_.store(true, std::memory_order_release);
    }

    void routine() {
        std::size_t step = 0;

        while (!done() || hasTasks()) {
            ++step;

            auto [status, task] = nextTask();

            if (!status.isOk())
                Backoff::backoff(step);
            else {
                invokeFunction(task);
                destroyFunction(task);

                step = 0;
            }
        }
    }

    bool hasTasks() const noexcept {
        return !taskQueue_.empty();
    }

    [[nodiscard]] std::tuple<Status, function_ptr> nextTask() {
        function_ptr cw;

        if (taskQueue_.pop(cw))
            return {Status::Ok(), cw};

        return {Status::InvalidOperation("Queue empty"), nullptr};
    }

    template <typename F>
    function_ptr createFunction(F&& f) {
        return new function{std::forward<F>(f)};
    }

    void destroyFunction(function_ptr fptr) noexcept {
        delete fptr;
    }

    void invokeFunction(function_ptr fptr) {
        if (!fptr)
            return;

        try {
            (*fptr)();
        }
        catch(...) {} // for now just ignoring exceptions
    }

    std::atomic<bool> done_;
    std::vector<std::thread> threadPool_;
    boost::lockfree::queue<function*,
                           boost::lockfree::fixed_sized<true>,
                           boost::lockfree::capacity<2048>> taskQueue_;
};

}
