#include <array>
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
    const std::size_t N_THREADS = 2 * std::thread::hardware_concurrency();
    static constexpr std::size_t PROPS_COUNT = 20000;
    static constexpr std::size_t LINKS_COUNT = 1000;

    void SetUp() override {
        removeFiles();

        volume1_ = std::make_shared<ondisk::Volume>();
        volume2_ = std::make_shared<ondisk::Volume>();
        volume3_ = std::make_shared<ondisk::Volume>();

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

    void createPath(std::shared_ptr<ondisk::Volume>& ptr, std::string_view path) {
        auto root = ptr->entry("/");

        ASSERT_TRUE(root != nullptr);

        const auto& tokens = util::split(util::simplifyPath(path), '/');
        std::string trackPath = "";

        for (const auto& token : tokens) {
            auto children = root->children();

            trackPath += ("/" + token);

            if (auto it = std::find(std::cbegin(children), std::cend(children), token); it != std::cend(children)) {
                auto handle = ptr->entry(trackPath);

                ASSERT_TRUE(handle != nullptr);

                std::swap(root, handle);

                continue;
            }
            else {
                auto status = ptr->link(*root, token);

                ASSERT_TRUE(status.isOk());

                auto handle = ptr->entry(trackPath);

                ASSERT_TRUE(status.isOk());

                std::swap(root, handle);
            }
        }
    }

    void doMounts() {
        createPath(volume1_, "/proc");
        createPath(volume2_, "/proc");
        createPath(volume3_, "/proc");

        ASSERT_TRUE(storage_.mount(volume1_, "/proc", "/",     Storage::MaxPriority).isOk());
        ASSERT_TRUE(storage_.mount(volume1_, "/proc", "/proc", Storage::MaxPriority).isOk());
        ASSERT_TRUE(storage_.mount(volume2_, "/proc", "/proc", Storage::DefaultPriority).isOk());
        ASSERT_TRUE(storage_.mount(volume3_, "/proc", "/proc", Storage::MinPriority).isOk());
    }

    void doUnmounts() {
        ASSERT_TRUE(storage_.unmount(volume1_, "/proc", "/proc").isOk());
        ASSERT_TRUE(storage_.unmount(volume2_, "/proc", "/proc").isOk());
        ASSERT_TRUE(storage_.unmount(volume3_, "/proc", "/proc").isOk());
        ASSERT_TRUE(storage_.unmount(volume1_, "/proc", "/").isOk());
    }

    std::string currentThreadIdString() {
        static constexpr std::hash<std::thread::id> hasher;

        return std::to_string(hasher(std::this_thread::get_id()));
    }

    std::shared_ptr<ondisk::Volume> volume1_;
    std::shared_ptr<ondisk::Volume> volume2_;
    std::shared_ptr<ondisk::Volume> volume3_;
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

    auto handle = volume1_->entry("/proc");

    ASSERT_NE(handle, nullptr);

    auto startTime = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < PROPS_COUNT; ++i) {
        auto status = handle->setProperty(propsNames[i % propsPool.size()], propsPool[i % propsPool.size()]);
        SKV_UNUSED(status);
    }

    auto stopTime = std::chrono::steady_clock::now();

    auto msElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stopTime - startTime).count();

    Log::i("SingleThreadVolume", "setProperty() elapsed time: ", msElapsed, " ms.");
    Log::i("SingleThreadVolume", "setProperty() speed: ", (1000.0 / msElapsed) * PROPS_COUNT, " prop/s");

    startTime = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < PROPS_COUNT; ++i) {
        const auto& [status, value] = handle->property(propsNames[i % propsPool.size()]);
        SKV_UNUSED(status);
        ASSERT_EQ(value, propsPool[i % propsPool.size()]);
    }

    stopTime = std::chrono::steady_clock::now();

    msElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(stopTime - startTime).count();

    Log::i("SingleThreadVolume", "getProperty() elapsed time: ", msElapsed, " ms.");
    Log::i("SingleThreadVolume", "getProperty() speed: ", (1000.0 / msElapsed) * PROPS_COUNT, " prop/s");

    doUnmounts();
}

TEST_F(VFSStoragePerfomanceTest, OneRecordMultipleThreadsReadWrite) {
    using namespace std::chrono;

    doMounts();

    auto proc = storage_.entry("/proc");

    ASSERT_NE(proc, nullptr);

    static std::atomic<bool> go_{false};

    auto writerRoutine = [proc{proc}, this] {
        while (!go_.load(std::memory_order_acquire))
            std::this_thread::yield();

        for (std::size_t i = 0; i < PROPS_COUNT; ++i) {
            auto status = proc->setProperty(propsNames[i % propsPool.size()], propsPool[i % propsPool.size()]);
            SKV_UNUSED(status);
        }
    };

    auto readerRoutine = [proc{proc}, this] {
        while (!go_.load(std::memory_order_acquire))
            std::this_thread::yield();

        for (std::size_t i = 0; i < PROPS_COUNT; ++i) {
            const auto& [status, value] = proc->property(propsNames[i % propsPool.size()]);

            SKV_UNUSED(status);
            SKV_UNUSED(value);
        }
    };

    for (size_t i = 1; i <= N_THREADS; ++i) {
        go_.store(false);

        std::vector<std::thread> threads;
        std::generate_n(std::back_inserter(threads), i, [&] { return std::thread(writerRoutine); });

        auto startTime = steady_clock::now();

        go_.store(true, std::memory_order_release);

        std::for_each(std::begin(threads), std::end(threads),
                      [](auto& t) { if (t.joinable()) t.join(); });

        auto stopTime = steady_clock::now();

        auto msElapsed = duration_cast<milliseconds>(stopTime - startTime).count();

        Log::i("OneRecordMultipleThreads [threads: " + std::to_string(i) + "]", "setProperty() elapsed time (per. thread): ", msElapsed, " ms.");
        Log::i("OneRecordMultipleThreads [threads: " + std::to_string(i) + "]", "setProperty() speed (per.thread): ", (1000.0 / msElapsed) * PROPS_COUNT, " prop/s");
    }

    for (size_t i = 1; i <= N_THREADS; ++i) {
        go_.store(false);

        std::vector<std::thread> threads;
        std::generate_n(std::back_inserter(threads), i, [&] { return std::thread(readerRoutine); });

        auto startTime = steady_clock::now();

        go_.store(true, std::memory_order_release);

        std::for_each(std::begin(threads), std::end(threads),
                      [](auto& t) { if (t.joinable()) t.join(); });

        auto stopTime = steady_clock::now();

        auto msElapsed = duration_cast<milliseconds>(stopTime - startTime).count();

        Log::i("OneRecordMultipleThreads [threads: " + std::to_string(i) + "]", "getProperty() elapsed time (per. thread): ", msElapsed, " ms.");
        Log::i("OneRecordMultipleThreads [threads: " + std::to_string(i) + "]", "getProperty() speed (per.thread): ", (1000.0 / msElapsed) * PROPS_COUNT, " prop/s");
    }

    doUnmounts();
}

TEST_F(VFSStoragePerfomanceTest, MultipleRecordsMultipleThreadsReadWrite) {
    using namespace std::chrono;

    doMounts();

    auto proc = storage_.entry("/proc");

    ASSERT_NE(proc, nullptr);

    for (size_t i = 1; i <= N_THREADS; ++i) {
        ASSERT_TRUE(storage_.link(*proc, std::to_string(i)).isOk());
    }

    static std::atomic<bool> go_{false};

    auto writerRoutine = [this](std::size_t id) {
        auto proc = storage_.entry("/proc/" + std::to_string(id));

        ASSERT_NE(proc, nullptr);

        while (!go_.load(std::memory_order_acquire))
            std::this_thread::yield();

        for (std::size_t i = 0; i < PROPS_COUNT; ++i) {
            auto status = proc->setProperty(propsNames[i % propsPool.size()], propsPool[i % propsPool.size()]);
            SKV_UNUSED(status);
        }
    };

    auto readerRoutine = [this](std::size_t id) {
        auto proc = storage_.entry("/proc/" + std::to_string(id));

        ASSERT_NE(proc, nullptr);

        while (!go_.load(std::memory_order_acquire))
            std::this_thread::yield();

        for (std::size_t i = 0; i < PROPS_COUNT; ++i) {
            const auto& [status, value] = proc->property(propsNames[i % propsPool.size()]);

            SKV_UNUSED(status);
            SKV_UNUSED(value);
        }
    };

    for (size_t i = 1; i <= N_THREADS; ++i) {
        go_.store(false);

        std::vector<std::thread> threads;

        for (std::size_t j = 1; j <= i; ++j)
            threads.emplace_back(writerRoutine, j);

        auto startTime = steady_clock::now();

        go_.store(true, std::memory_order_release);

        std::for_each(std::begin(threads), std::end(threads),
                      [](auto& t) { if (t.joinable()) t.join(); });

        auto stopTime = steady_clock::now();

        auto msElapsed = duration_cast<milliseconds>(stopTime - startTime).count();

        Log::i("MultipleRecordsMultipleThreads [threads: " + std::to_string(i) + "]", "setProperty() elapsed time (per. thread): ", msElapsed, " ms.");
        Log::i("MultipleRecordsMultipleThreads [threads: " + std::to_string(i) + "]", "setProperty() speed (per.thread): ", (1000.0 / msElapsed) * PROPS_COUNT, " prop/s");
    }

    for (size_t i = 1; i <= N_THREADS; ++i) {
        go_.store(false);

        std::vector<std::thread> threads;

        for (std::size_t j = 1; j <= i; ++j)
            threads.emplace_back(readerRoutine, j);

        auto startTime = steady_clock::now();

        go_.store(true, std::memory_order_release);

        std::for_each(std::begin(threads), std::end(threads),
                      [](auto& t) { if (t.joinable()) t.join(); });

        auto stopTime = steady_clock::now();

        auto msElapsed = duration_cast<milliseconds>(stopTime - startTime).count();

        Log::i("MultipleRecordsMultipleThreads [threads: " + std::to_string(i) + "]", "getProperty() elapsed time (per. thread): ", msElapsed, " ms.");
        Log::i("MultipleRecordsMultipleThreads [threads: " + std::to_string(i) + "]", "getProperty() speed (per.thread): ", (1000.0 / msElapsed) * PROPS_COUNT, " prop/s");
    }

    {   // inspecting storage links
        auto children = proc->children();

        ASSERT_EQ(children.size(), N_THREADS);

        for (const auto& child : children) {
            auto path = util::simplifyPath("/proc/" + child);

            auto chandle = storage_.entry(path);

            ASSERT_NE(chandle, nullptr);

            auto props = chandle->properties();

            ASSERT_EQ(props.size(), propsPool.size());

            for (std::size_t i = 0; i < propsNames.size(); ++i)
                ASSERT_EQ(props[propsNames[i]], propsPool[i]);
        }
    }

    doUnmounts();
}

TEST_F(VFSStoragePerfomanceTest, RemovePropertyRecordTest) {
    using namespace std::chrono;

    doMounts();

    auto proc = storage_.entry("/proc");

    ASSERT_NE(proc, nullptr);

    for (size_t i = 1; i <= N_THREADS; ++i) {
        ASSERT_TRUE(storage_.link(*proc, std::to_string(i)).isOk());
    }

    static std::atomic<bool> go_{false};

    auto removeRoutine = [this](std::size_t id) {
        auto handle = storage_.entry("/proc/" + std::to_string(id));

        ASSERT_NE(handle, nullptr);

        while (!go_.load(std::memory_order_acquire))
            std::this_thread::yield();

        for (std::size_t i = 0; i < PROPS_COUNT; ++i) {
            auto status = handle->removeProperty(std::to_string(i));
            SKV_UNUSED(status);
        }
    };

    for (size_t i = 1; i <= N_THREADS; ++i) {
        go_.store(false);

        for (size_t j = 1; j <= i; ++j) {
            auto handle = storage_.entry("/proc/" + std::to_string(j));

            ASSERT_NE(handle, nullptr);

            {
                const auto &propNames = handle->propertiesNames();
                ASSERT_TRUE(propNames.empty());
            }

            for (std::size_t i = 0; i < PROPS_COUNT; ++i) {
                auto status = handle->setProperty(std::to_string(i), propsPool[i % propsPool.size()]);
                ASSERT_TRUE(status.isOk());
            }
        }

        std::vector<std::thread> threads;

        for (std::size_t j = 1; j <= i; ++j)
            threads.emplace_back(removeRoutine, j);

        auto startTime = steady_clock::now();

        go_.store(true, std::memory_order_release);

        std::for_each(std::begin(threads), std::end(threads),
                      [](auto& t) { if (t.joinable()) t.join(); });

        auto stopTime = steady_clock::now();

        auto msElapsed = duration_cast<milliseconds>(stopTime - startTime).count();

        Log::i("RemovePropertyRecordTest [threads: " + std::to_string(i) + "]", "removeProperty() elapsed time (per. thread): ", msElapsed, " ms.");
        Log::i("RemovePropertyRecordTest [threads: " + std::to_string(i) + "]", "removeProperty() speed (per.thread): ", (1000.0 / msElapsed) * PROPS_COUNT, " prop/s");
    }

    doUnmounts();
}


TEST_F(VFSStoragePerfomanceTest, LinkUnlinkRecordTest) {
    using namespace std::chrono;

    doMounts();

    auto proc = storage_.entry("/proc");

    ASSERT_NE(proc, nullptr);

    auto startTime = steady_clock::now();

    for (std::size_t i = 0; i < LINKS_COUNT; ++i)
        ASSERT_TRUE(storage_.link(*proc, std::to_string(i)).isOk());

    auto stopTime = steady_clock::now();

    auto msElapsed = duration_cast<milliseconds>(stopTime - startTime).count();

    Log::i("CreateRecordTest", "link() elapsed time: ", msElapsed, " ms.");
    Log::i("CreateRecordTest", "link() speed: ", (1000.0 / msElapsed) * LINKS_COUNT, " link/s");

    auto links = proc->children();

    ASSERT_EQ(links.size(), LINKS_COUNT);

    startTime = steady_clock::now();

    for (std::size_t i = 0; i < LINKS_COUNT; ++i)
        ASSERT_TRUE(storage_.unlink(*proc, std::to_string(i)).isOk());

    stopTime = steady_clock::now();

    msElapsed = duration_cast<milliseconds>(stopTime - startTime).count();

    Log::i("CreateRecordTest", "unlink() elapsed time: ", msElapsed, " ms.");
    Log::i("CreateRecordTest", "unlink() speed: ", (1000.0 / msElapsed) * LINKS_COUNT, " link/s");

    doUnmounts();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
