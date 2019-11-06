#include <atomic>
#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include <ondisk/Volume.hpp>
#include <os/File.hpp>
#include <vfs/Storage.hpp>
#include <util/String.hpp>
#include <util/StringPath.hpp>
#include <util/Log.hpp>
#include <util/Unused.hpp>

using namespace skv;
using namespace skv::vfs;
using namespace skv::util;

namespace chrono = std::chrono;

using namespace std::literals;

class VFSStoragePerfomanceTest: public ::testing::Test {
#ifdef BUILDING_UNIX
    const std::string VOLUME_DIR  = "/tmp";
    const std::string VOLUME_N1_NAME = "volume1";
    const std::string VOLUME_N2_NAME = "volume2";
    const std::string VOLUME_N3_NAME = "volume3";
#else
    const std::string VOLUME_DIR  = ".";
    const std::string VOLUME_N1_NAME = "volume1";
    const std::string VOLUME_N2_NAME = "volume2";
    const std::string VOLUME_N3_NAME = "volume3";
#endif

protected:
    static constexpr std::size_t PROPS_COUNT = 100000;

    void SetUp() override {
        removeFiles();

        volume1_ = ondisk::make_ondisk_volume();
        volume2_ = ondisk::make_ondisk_volume();
        volume3_ = ondisk::make_ondisk_volume();

        ASSERT_NE(volume1_, nullptr);
        ASSERT_NE(volume2_, nullptr);
        ASSERT_NE(volume3_, nullptr);

        ASSERT_TRUE(volume1_->initialize(VOLUME_DIR, VOLUME_N1_NAME).isOk());
        ASSERT_TRUE(volume2_->initialize(VOLUME_DIR, VOLUME_N2_NAME).isOk());
        ASSERT_TRUE(volume3_->initialize(VOLUME_DIR, VOLUME_N3_NAME).isOk());
    }

    void TearDown() override {
        ASSERT_TRUE(volume3_->deinitialize().isOk());
        ASSERT_TRUE(volume2_->deinitialize().isOk());
        ASSERT_TRUE(volume1_->deinitialize().isOk());

        volume3_.reset();
        volume2_.reset();
        volume1_.reset();

        removeFiles();
    }

    void removeFiles() {
        SKV_UNUSED(os::File::unlink(VOLUME_DIR + os::File::sep() + VOLUME_N1_NAME + ".logd"));
        SKV_UNUSED(os::File::unlink(VOLUME_DIR + os::File::sep() + VOLUME_N1_NAME+ ".index"));
        SKV_UNUSED(os::File::unlink(VOLUME_DIR + os::File::sep() + VOLUME_N2_NAME + ".logd"));
        SKV_UNUSED(os::File::unlink(VOLUME_DIR + os::File::sep() + VOLUME_N2_NAME+ ".index"));
        SKV_UNUSED(os::File::unlink(VOLUME_DIR + os::File::sep() + VOLUME_N3_NAME + ".logd"));
        SKV_UNUSED(os::File::unlink(VOLUME_DIR + os::File::sep() + VOLUME_N3_NAME+ ".index"));
    }

    void createPath(IVolumePtr& ptr, std::string_view path) {
        auto [status, rootHandle] = ptr->open("/");

        ASSERT_TRUE(status.isOk());

        const auto& tokens = util::split(util::simplifyPath(path), '/');
        std::string trackPath = "";

        for (const auto& token : tokens) {
            auto [status, children] = ptr->links(rootHandle);

            trackPath += ("/" + token);

            if (auto it = std::find(std::cbegin(children), std::cend(children), token); it != std::cend(children)) {
                auto [status, handle] = ptr->open(trackPath);

                ASSERT_TRUE(status.isOk());

                std::swap(rootHandle, handle);

                ASSERT_TRUE(ptr->close(handle).isOk());

                continue;
            }
            else {
                status = ptr->link(rootHandle, token);

                ASSERT_TRUE(status.isOk());

                auto [status, handle] = ptr->open(trackPath);

                ASSERT_TRUE(status.isOk());

                std::swap(rootHandle, handle);

                ASSERT_TRUE(ptr->close(handle).isOk());
            }
        }

        ASSERT_TRUE(ptr->close(rootHandle).isOk());
    }

    void doMounts() {
        createPath(volume1_, "/proc");
        createPath(volume2_, "/proc");
        createPath(volume3_, "/proc");

        ASSERT_TRUE(storage_.mount(volume1_, "/",     "/",     Storage::MaxPriority).isOk());
        ASSERT_TRUE(storage_.mount(volume1_, "/proc", "/proc", Storage::MaxPriority).isOk());
        ASSERT_TRUE(storage_.mount(volume2_, "/proc", "/proc", Storage::DefaultPriority).isOk());
        ASSERT_TRUE(storage_.mount(volume3_, "/proc", "/proc", Storage::MinPriority).isOk());
    }

    void doUnmounts() {
        ASSERT_TRUE(storage_.unmount(volume1_, "/proc", "/proc").isOk());
        ASSERT_TRUE(storage_.unmount(volume2_, "/proc", "/proc").isOk());
        ASSERT_TRUE(storage_.unmount(volume3_, "/proc", "/proc").isOk());
        ASSERT_TRUE(storage_.unmount(volume1_, "/",     "/").isOk());
    }

    std::string currentThreadIdString() {
        static constexpr std::hash<std::thread::id> hasher;

        return std::to_string(hasher(std::this_thread::get_id()));
    }

    IVolumePtr volume1_;
    IVolumePtr volume2_;
    IVolumePtr volume3_;
    vfs::Storage storage_;

    std::array<Property, 7> propsPool{Property{123.0f},
                                      Property{956.0},
                                      Property{std::uint8_t{20}},
                                      Property{std::uint32_t{1024 * 1024 * 1024}},
                                      Property{std::uint64_t{1024} * 1024 * 1024 * 1024 * 1024},
                                      Property{std::string(256, 'a')},
                                      Property{std::vector<char>(1024, 'Z')}};
    std::array<std::string, 7> propsNames{"flt_prop",
                                          "double_prop",
                                          "uint8t_prop",
                                          "uint32t_prop",
                                          "uint64t_prop",
                                          "string_prop",
                                          "blob_prop"};
};

TEST_F(VFSStoragePerfomanceTest, SingleThread_VolumeOnly) {
    doMounts();

    auto [status, handle] = volume1_->open("/proc");

    ASSERT_TRUE(status.isOk());

    auto startTime = chrono::steady_clock::now();

    for (std::size_t i = 0; i < PROPS_COUNT; ++i) {
        auto status = volume1_->setProperty(handle, propsNames[i % propsPool.size()], propsPool[i % propsPool.size()]);
        SKV_UNUSED(status);
    }

    auto stopTime = chrono::steady_clock::now();

    auto msElapsed = chrono::duration_cast<chrono::milliseconds>(stopTime - startTime).count();

    Log::i("SingleThreadVolume", "setProperty() elapsed time: ", msElapsed, " ms.");
    Log::i("SingleThreadVolume", "setProperty() speed: ", (1000.0 / msElapsed) * PROPS_COUNT, " prop/s");

    startTime = chrono::steady_clock::now();

    for (std::size_t i = 0; i < PROPS_COUNT; ++i) {
        const auto& [status, value] = volume1_->property(handle, propsNames[i % propsPool.size()]);

        ASSERT_EQ(value, propsPool[i % propsPool.size()]);
    }

    stopTime = chrono::steady_clock::now();

    msElapsed = chrono::duration_cast<chrono::milliseconds>(stopTime - startTime).count();

    Log::i("SingleThreadVolume", "getProperty() elapsed time: ", msElapsed, " ms.");
    Log::i("SingleThreadVolume", "getProperty() speed: ", (1000.0 / msElapsed) * PROPS_COUNT, " prop/s");

    ASSERT_TRUE(volume1_->close(handle).isOk());

    doUnmounts();
}

TEST_F(VFSStoragePerfomanceTest, OneRecordMultipleThreads) {
    doMounts();

    auto [status, handle] = storage_.open("/proc");

    ASSERT_TRUE(status.isOk());

    static std::atomic<bool> go_{false};

    auto writerRoutine = [handle{handle}, this] {
        while (!go_.load(std::memory_order_acquire))
            std::this_thread::yield();

        for (std::size_t i = 0; i < PROPS_COUNT; ++i) {
            auto status = storage_.setProperty(handle, propsNames[i % propsPool.size()], propsPool[i % propsPool.size()]);
            SKV_UNUSED(status);
        }
    };

    auto readerRoutine = [handle{handle}, this] {
        while (!go_.load(std::memory_order_acquire))
            std::this_thread::yield();

        for (std::size_t i = 0; i < PROPS_COUNT; ++i) {
            const auto& [status, value] = storage_.property(handle, propsNames[i % propsPool.size()]);

            SKV_UNUSED(status);
            SKV_UNUSED(value);
        }
    };

    for (size_t i = 1; i <= std::thread::hardware_concurrency(); ++i) {
        go_.store(false);

        std::vector<std::thread> threads;
        std::generate_n(std::back_inserter(threads), i, [&] { return std::thread(writerRoutine); });

        auto startTime = chrono::steady_clock::now();

        go_.store(true, std::memory_order_release);

        std::for_each(std::begin(threads), std::end(threads),
                      [](auto& t) { if (t.joinable()) t.join(); });

        auto stopTime = chrono::steady_clock::now();

        auto msElapsed = chrono::duration_cast<chrono::milliseconds>(stopTime - startTime).count();

        Log::i("OneRecordMultipleThreads [threads: " + std::to_string(i) + "]", "setProperty() elapsed time (per. thread): ", msElapsed, " ms.");
        Log::i("OneRecordMultipleThreads [threads: " + std::to_string(i) + "]", "setProperty() speed (per.thread): ", (1000.0 / msElapsed) * PROPS_COUNT, " prop/s");
    }

    for (size_t i = 1; i <= std::thread::hardware_concurrency(); ++i) {
        go_.store(false);

        std::vector<std::thread> threads;
        std::generate_n(std::back_inserter(threads), i, [&] { return std::thread(readerRoutine); });

        auto startTime = chrono::steady_clock::now();

        go_.store(true, std::memory_order_release);

        std::for_each(std::begin(threads), std::end(threads),
                      [](auto& t) { if (t.joinable()) t.join(); });

        auto stopTime = chrono::steady_clock::now();

        auto msElapsed = chrono::duration_cast<chrono::milliseconds>(stopTime - startTime).count();

        Log::i("OneRecordMultipleThreads [threads: " + std::to_string(i) + "]", "getProperty() elapsed time (per. thread): ", msElapsed, " ms.");
        Log::i("OneRecordMultipleThreads [threads: " + std::to_string(i) + "]", "getProperty() speed (per.thread): ", (1000.0 / msElapsed) * PROPS_COUNT, " prop/s");
    }

    ASSERT_TRUE(storage_.close(handle).isOk());

    doUnmounts();
}

TEST_F(VFSStoragePerfomanceTest, MultipleRecordsMultipleThreads) {
    using namespace chrono;
    doMounts();

    auto [status, handle] = storage_.open("/proc");

    ASSERT_TRUE(status.isOk());

    for (size_t i = 1; i <= std::thread::hardware_concurrency(); ++i) {
        ASSERT_TRUE(storage_.link(handle, std::to_string(i)).isOk());
    }

    static std::atomic<bool> go_{false};

    auto writerRoutine = [this](std::size_t id) {
        auto [status, handle] = storage_.open("/proc/" + std::to_string(id));

        ASSERT_TRUE(status.isOk());

        while (!go_.load(std::memory_order_acquire))
            std::this_thread::yield();

        for (std::size_t i = 0; i < PROPS_COUNT; ++i) {
            auto status = storage_.setProperty(handle, propsNames[i % propsPool.size()], propsPool[i % propsPool.size()]);
            SKV_UNUSED(status);
        }

        ASSERT_TRUE(storage_.close(handle).isOk());
    };

    auto readerRoutine = [this](std::size_t id) {
        auto [status, handle] = storage_.open("/proc/" + std::to_string(id));

        ASSERT_TRUE(status.isOk());

        while (!go_.load(std::memory_order_acquire))
            std::this_thread::yield();

        for (std::size_t i = 0; i < PROPS_COUNT; ++i) {
            const auto& [status, value] = storage_.property(handle, propsNames[i % propsPool.size()]);

            SKV_UNUSED(status);
            SKV_UNUSED(value);
        }

        ASSERT_TRUE(storage_.close(handle).isOk());
    };

    for (size_t i = 1; i <= std::thread::hardware_concurrency(); ++i) {
        go_.store(false);

        std::vector<std::thread> threads;

        for (std::size_t j = 1; j <= i; ++j)
            threads.emplace_back(writerRoutine, j);

        auto startTime = chrono::steady_clock::now();

        go_.store(true, std::memory_order_release);

        std::for_each(std::begin(threads), std::end(threads),
                      [](auto& t) { if (t.joinable()) t.join(); });

        auto stopTime = chrono::steady_clock::now();

        auto msElapsed = chrono::duration_cast<chrono::milliseconds>(stopTime - startTime).count();

        Log::i("MultipleRecordsMultipleThreads [threads: " + std::to_string(i) + "]", "setProperty() elapsed time (per. thread): ", msElapsed, " ms.");
        Log::i("MultipleRecordsMultipleThreads [threads: " + std::to_string(i) + "]", "setProperty() speed (per.thread): ", (1000.0 / msElapsed) * PROPS_COUNT, " prop/s");
    }

    for (size_t i = 1; i <= std::thread::hardware_concurrency(); ++i) {
        go_.store(false);

        std::vector<std::thread> threads;

        for (std::size_t j = 1; j <= i; ++j)
            threads.emplace_back(readerRoutine, j);

        auto startTime = chrono::steady_clock::now();

        go_.store(true, std::memory_order_release);

        std::for_each(std::begin(threads), std::end(threads),
                      [](auto& t) { if (t.joinable()) t.join(); });

        auto stopTime = chrono::steady_clock::now();

        auto msElapsed = chrono::duration_cast<chrono::milliseconds>(stopTime - startTime).count();

        Log::i("MultipleRecordsMultipleThreads [threads: " + std::to_string(i) + "]", "getProperty() elapsed time (per. thread): ", msElapsed, " ms.");
        Log::i("MultipleRecordsMultipleThreads [threads: " + std::to_string(i) + "]", "getProperty() speed (per.thread): ", (1000.0 / msElapsed) * PROPS_COUNT, " prop/s");
    }

    {
        auto [status, links] = storage_.links(handle);

        ASSERT_TRUE(status.isOk());
    }

    ASSERT_TRUE(storage_.close(handle).isOk());

    doUnmounts();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
