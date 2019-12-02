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

template <typename BackoffStrategy = FixedStepBackoff<>>
class SpinLock final {
public:
    void lock() noexcept {
        bool isLocked{false};
        std::size_t step{0};

        while (!locked_.compare_exchange_weak(isLocked,
                                              true,
                                              std::memory_order_acquire,
                                              std::memory_order_relaxed)) {
            BackoffStrategy::backoff(++step);

            isLocked = false;
        }
    }

    void unlock() noexcept {
        locked_.store(false, std::memory_order_release);
    }

private:
    std::atomic<bool> locked_{false};
};

}
