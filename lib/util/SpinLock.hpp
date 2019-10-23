#pragma once

#include <atomic>

namespace skv::util {

class SpinLock final {
public:
    void lock() noexcept;
    void unlock() noexcept;

private:
    std::atomic<bool> locked_{false};
};

}
