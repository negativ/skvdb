#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

#include <boost/lockfree/queue.hpp>

#include "util/Status.hpp"

namespace skv::util {

template <std::size_t BACKOFF_STEPS = 64, std::size_t SLEEP_PREIOD_MS = 50>
class ThreadPool final {
    struct Callable {
        Callable() noexcept = default;
        virtual ~Callable() noexcept = default;

        virtual void call() = 0;
    };

    class CallWrapper {
        template <typename F>
        class Call final: public Callable {
        public:
            Call(F&& f):
                func_{std::move(f)}
            {}

            ~Call() noexcept override = default;

            void call() override { func_(); }
        private:
            F func_;
        };

    public:
        CallWrapper() noexcept  = default;

        template <typename F>
        CallWrapper(F&& f):
            impl_{new Call{std::move(f)}}
        {}

        ~CallWrapper() noexcept = default;

        CallWrapper(const CallWrapper&) = delete;
        CallWrapper& operator=(const CallWrapper&) = delete;

        CallWrapper(CallWrapper&& other) noexcept {
            using std::swap;

            swap(impl_, other.impl_);
        }

        CallWrapper& operator=(CallWrapper&& other) noexcept {
            using std::swap;

            swap(impl_, other.impl_);

            return *this;
        }

        void operator()() {
            impl_->call();
        }

    private:
        std::unique_ptr<Callable> impl_;
    };

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

    ~ThreadPool() noexcept {
        markDone();

        while (!taskQueue_.empty()) {
            CallWrapper *wrapper;

            if (taskQueue_.pop(wrapper))
                delete wrapper;
        }
    }

    template <typename F>
    decltype(auto) schedule(F&& f) {
        using result_t = std::invoke_result_t<F>;

        std::packaged_task<result_t()> packagedTask{std::move(f)};
        auto future{packagedTask.get_future()};

        CallWrapper *task = new CallWrapper(std::move(packagedTask));

        while (!taskQueue_.push(task));

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
            threads_{threads}
        {}

        ~Joiner() {
            std::for_each(std::begin(threads_), std::end(threads_),
                          [](auto& t) {
                              if (t.joinable())
                                  t.join();
                          });
        }

    private:
        container_type& threads_;
    };

    void markDone() {
        done_.store(true, std::memory_order_release);
    }

    void routine() {
        using namespace std::literals;

        constexpr std::size_t MAX_STEPS = 1000;
        constexpr auto SLEEP_PERIOD = 100ms;

        std::size_t step = 0;

        while (!done()) {
            ++step;

            auto [status, task] = nextTask();

            if (!status.isOk()) {
                if (step == MAX_STEPS) {
                    std::this_thread::sleep_for(SLEEP_PERIOD);
                    step = 0;
                }
                else
                    std::this_thread::yield();
            }
            else {
                task();

                step = 0;
            }
        }
    }

    std::tuple<Status, CallWrapper> nextTask() {
        CallWrapper* task;

        if (taskQueue_.pop(task)) {
            auto ret = std::move(*task);

            delete task;

            return {Status::Ok(), ret};
        }

        return {Status::InvalidOperation("Task queue empty"), CallWrapper{}};
    }

    std::atomic<bool> done_;
    std::vector<std::thread> threadPool_;
    Joiner joiner_;
    boost::lockfree::queue<CallWrapper*,
                           boost::lockfree::fixed_sized<true>,
                           boost::lockfree::capacity<2048>> taskQueue_;
};

}
