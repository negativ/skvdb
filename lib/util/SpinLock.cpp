#include "SpinLock.hpp"

namespace skv::util {

void SpinLock::lock() noexcept {
    bool isLocked;

    do {
        isLocked = false;
    } while (!locked_.compare_exchange_weak(isLocked,
                                            true,
                                            std::memory_order_acquire,
                                            std::memory_order_relaxed));
}

void SpinLock::unlock() noexcept {
    locked_.store(false, std::memory_order_release);
}



}
