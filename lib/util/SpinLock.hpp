#pragma once

#include <atomic>
#include <cstdint>
#include <chrono>
#include <thread>

namespace skv::util {

struct NoBackoff {
    static void backoff([[maybe_unused]] std::size_t step) {}
};

template <std::size_t Steps = 10000>
struct FixedStepBackoff {
    static void backoff([[maybe_unused]] std::size_t step) {
        if (step % Steps == 0)
            std::this_thread::yield();
    }
};

template <std::size_t Steps = 10000, std::size_t SleepMs = 50>
struct FixedStepSleepBackoff {
    static void backoff(std::size_t step) {
        if (step % Steps == 0)
            std::this_thread::sleep_for(std::chrono::milliseconds{SleepMs});
        else
            std::this_thread::yield();
    }
};

template <typename BackoffStrategy = FixedStepBackoff<>>
class SpinLock final {
public:
    void lock() noexcept {
        bool isLocked;
        std::size_t step = 0;

        do {
            BackoffStrategy::backoff(++step);

            isLocked = false;
        } while (!locked_.compare_exchange_weak(isLocked,
                                                true,
                                                std::memory_order_acquire,
                                                std::memory_order_relaxed));
    }

    void unlock() noexcept {
        locked_.store(false, std::memory_order_release);
    }

private:
    std::atomic<bool> locked_{false};
};

}
