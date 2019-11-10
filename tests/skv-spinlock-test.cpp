#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include <gtest/gtest.h>

#include "util/SpinLock.hpp"

namespace {
    skv::util::SpinLock<skv::util::NoBackoff> lock_;
    std::atomic<bool> go{false};
    unsigned int value;
    const auto TIMES = 10u;
}

using namespace std::literals;

void routine() {
    while (!go.load(std::memory_order_acquire))
        std::this_thread::yield();

    std::lock_guard locker(lock_);

    ++value;
}

TEST(SpinLockTest, TwoThreadTest) {
    value = 0;

    go.store(false);

    std::thread t1{&routine};
    std::thread t2{&routine};

    std::this_thread::sleep_for(1ms);

    go.store(true, std::memory_order_release);

    t1.join();
    t2.join();

    ASSERT_EQ(value, 2);
}

TEST(SpinLockTest, MaxHWThreadTestNtimes) {
    for (auto t = 0u; t < TIMES; ++t) {
        value = 0;

        std::vector<std::thread> threads;

        const auto N = std::thread::hardware_concurrency();

        ASSERT_TRUE(N > 0);

        for (auto i = 0u; i < N; ++i)
            threads.emplace_back(&routine);

        go.store(true, std::memory_order_release);

        std::this_thread::sleep_for(100ms);

        std::for_each(begin(threads), end(threads),
                      [](auto& t) { t.join(); });

        go.store(false);

        ASSERT_EQ(value, threads.size());
    }
}

TEST(SpinLockTest, OverSubscribtionThreadTestNtimes) {
    for (auto t = 0u; t < TIMES; ++t) {
        value = 0;

        std::vector<std::thread> threads;

        const auto N = std::thread::hardware_concurrency() * 4;

        ASSERT_TRUE(N > 0);

        for (auto i = 0u; i < N; ++i)
            threads.emplace_back(&routine);

        go.store(true, std::memory_order_release);

        std::this_thread::sleep_for(500ms);

        std::for_each(begin(threads), end(threads),
                      [](auto& t) { t.join(); });

        go.store(false);

        ASSERT_EQ(value, threads.size());
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

