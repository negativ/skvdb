#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <ondisk/Volume.hpp>
#include <os/File.hpp>
#include <util/Log.hpp>

using namespace skv;
using namespace skv::ondisk;

namespace {
#ifdef BUILDING_UNIX
    const std::string STORAGE_DIR  = "/tmp";
    const std::string STORAGE_NAME = "test_storage";
#else
    const char * const STORAGE_DIR  = ".";
    const char * const STORAGE_NAME = "test_storage";
#endif
    std::atomic<bool> go{false};
}

void mtTestRoutine(std::reference_wrapper<Volume> v) {
    static constexpr std::hash<std::thread::id> hasher;

    while (!go.load(std::memory_order_acquire))
        std::this_thread::yield();

    Volume &volume = v;

    auto [status, rootHandle] = volume.open("/");

    if (!status.isOk())
        return;

    {
        auto id = std::to_string(hasher(std::this_thread::get_id()));
        volume.link(rootHandle, id);

        auto path = "/" + id;

        auto [status, self] = volume.open(path);

        if (!status.isOk()) {
            return;
        }

        for (size_t i = 0; i < 10000; ++i) {
            if (i % 7 == 0)
                volume.setProperty(self, std::to_string(i), Property{double(i)});
            else if (i % 5 == 0) {
                volume.setProperty(self, std::to_string(i), Property{std::to_string(i)});
            }
            else if (i % 3 == 0) {
                volume.setProperty(self, std::to_string(i), Property{float(i)});
            }
            else
                volume.setProperty(self, std::to_string(i), Property{"fizz buzz"});
        }

        volume.close(self);
        volume.close(rootHandle);
    }
}

TEST(VolumeTest, MTTest) {
    using namespace std::chrono;
    using namespace std::literals;

    Volume volume;

    os::File::unlink(STORAGE_DIR + "/" + STORAGE_NAME + ".logd");
    os::File::unlink(STORAGE_DIR + "/" + STORAGE_NAME + ".index");

    auto status = volume.initialize(STORAGE_DIR, STORAGE_NAME);

    ASSERT_TRUE(status.isOk());
    ASSERT_TRUE(volume.initialized());

    std::vector<std::thread> threads;
    std::generate_n(std::back_inserter(threads),
                    2 * std::thread::hardware_concurrency(),
                    [&] { return std::thread{&mtTestRoutine, std::ref(volume)}; });

    std::this_thread::sleep_for(100ms); // ensure all other threads is started

    auto startTime = system_clock::now();

    go.store(true, std::memory_order_release);

    std::for_each(std::begin(threads), std::end(threads),
                  [](auto&& t) {
                      if (t.joinable())
                          t.join();
                      });

    auto endTime = system_clock::now();

    Log::i("MTTest", "Test took ", duration_cast<milliseconds>(endTime - startTime).count(), " ms");

    ASSERT_TRUE(volume.deinitialize().isOk());
    ASSERT_FALSE(volume.initialized());
}

TEST(VolumeTest, OpenCloseLink) {
    Volume volume;

    os::File::unlink(STORAGE_DIR + "/" + STORAGE_NAME + ".logd");
    os::File::unlink(STORAGE_DIR + "/" + STORAGE_NAME + ".index");

    auto status = volume.initialize(STORAGE_DIR, STORAGE_NAME);

    ASSERT_TRUE(status.isOk());
    ASSERT_TRUE(volume.initialized());

    {
        auto [status, rootHandle] = volume.open("/");

        ASSERT_TRUE(status.isOk());

        status = volume.link(rootHandle, "dev");
        ASSERT_TRUE(status.isOk());

        status = volume.link(rootHandle, "proc");
        ASSERT_TRUE(status.isOk());

        status = volume.link(rootHandle, "usr");
        ASSERT_TRUE(status.isOk());

        {
            auto [status, children] = volume.links(rootHandle);

            ASSERT_EQ(children.size(), 3);
        }

        volume.close(rootHandle);
    }

    {
        auto [status, procHandle] = volume.open("/proc");

        ASSERT_TRUE(status.isOk());

        status = volume.link(procHandle, "1");
        ASSERT_TRUE(status.isOk());

        status = volume.link(procHandle, "2");
        ASSERT_TRUE(status.isOk());

        {
            auto [status, children] = volume.links(procHandle);

            ASSERT_EQ(children.size(), 2);
        }

        volume.close(procHandle);
    }

    {
        auto [status, procHandle] = volume.open("/proc");

        {
            auto [status, children] = volume.links(procHandle);

            ASSERT_EQ(children.size(), 2);
        }

        status = volume.link(procHandle, "self");
        ASSERT_TRUE(status.isOk());

        status = volume.link(procHandle, "self");
        ASSERT_FALSE(status.isOk());

        ASSERT_TRUE(volume.close(procHandle).isOk());
    }

    {
        auto [status, selfHandle] = volume.open("/proc/self");

        {
            auto [status, children] = volume.links(selfHandle);

            ASSERT_TRUE(children.empty());
        }

        ASSERT_TRUE(volume.setProperty(selfHandle, "int_property", 1).isOk());
        ASSERT_TRUE(volume.setProperty(selfHandle, "str_property", "some text").isOk());
        ASSERT_TRUE(volume.setProperty(selfHandle, "flt_property", 1024.0f).isOk());
        ASSERT_TRUE(volume.setProperty(selfHandle, "dbl_property", 4096.0).isOk());

        ASSERT_TRUE(volume.close(selfHandle).isOk());
    }

    {
        auto [status, selfHandle] = volume.open("/proc/self");

        {
            auto [status, children] = volume.links(selfHandle);

            ASSERT_TRUE(children.empty());
        }

        auto [s1, b1] = volume.hasProperty(selfHandle, "int_property");
        auto [s2, b2] = volume.hasProperty(selfHandle, "str_property");
        auto [s3, b3] = volume.hasProperty(selfHandle, "flt_property");
        auto [s4, b4] = volume.hasProperty(selfHandle, "dbl_property");

        ASSERT_TRUE(s1.isOk() && b1);
        ASSERT_TRUE(s2.isOk() && b2);
        ASSERT_TRUE(s3.isOk() && b3);
        ASSERT_TRUE(s4.isOk() && b4);

        ASSERT_TRUE(volume.close(selfHandle).isOk());
    }

    {
        auto [status, selfHandle] = volume.open("/proc/self");

        auto [s1, v1] = volume.property(selfHandle, "int_property");
        auto [s2, v2] = volume.property(selfHandle, "str_property");
        auto [s3, v3] = volume.property(selfHandle, "flt_property");
        auto [s4, v4] = volume.property(selfHandle, "dbl_property");

        ASSERT_TRUE(s1.isOk() && v1 == Property{1});
        ASSERT_TRUE(s2.isOk() && v2 == Property{"some text"});
        ASSERT_TRUE(s3.isOk() && v3 == Property{1024.0f});
        ASSERT_TRUE(s4.isOk() && v4 == Property{4096.0});

        ASSERT_TRUE(volume.close(selfHandle).isOk());
    }

    ASSERT_TRUE(volume.deinitialize().isOk());
    ASSERT_FALSE(volume.initialized());

    status = volume.initialize(STORAGE_DIR, STORAGE_NAME);

    ASSERT_TRUE(status.isOk());
    ASSERT_TRUE(volume.initialized());

    {
        auto [status, selfHandle] = volume.open("/proc/self");

        {
            auto [status, children] = volume.links(selfHandle);

            ASSERT_TRUE(children.empty());
        }

        auto [s1, b1] = volume.hasProperty(selfHandle, "int_property");
        auto [s2, b2] = volume.hasProperty(selfHandle, "str_property");
        auto [s3, b3] = volume.hasProperty(selfHandle, "flt_property");
        auto [s4, b4] = volume.hasProperty(selfHandle, "dbl_property");

        ASSERT_TRUE(s1.isOk() && b1);
        ASSERT_TRUE(s2.isOk() && b2);
        ASSERT_TRUE(s3.isOk() && b3);
        ASSERT_TRUE(s4.isOk() && b4);

        ASSERT_TRUE(volume.close(selfHandle).isOk());
    }

    {
        auto [status, selfHandle] = volume.open("/proc/self");

        auto [s1, v1] = volume.property(selfHandle, "int_property");
        auto [s2, v2] = volume.property(selfHandle, "str_property");
        auto [s3, v3] = volume.property(selfHandle, "flt_property");
        auto [s4, v4] = volume.property(selfHandle, "dbl_property");

        ASSERT_TRUE(s1.isOk() && v1 == Property{1});
        ASSERT_TRUE(s2.isOk() && v2 == Property{"some text"});
        ASSERT_TRUE(s3.isOk() && v3 == Property{1024.0f});
        ASSERT_TRUE(s4.isOk() && v4 == Property{4096.0});

        ASSERT_TRUE(volume.close(selfHandle).isOk());
    }

    ASSERT_TRUE(volume.deinitialize().isOk());
    ASSERT_FALSE(volume.initialized());

    os::File::unlink(STORAGE_DIR + "/" + STORAGE_NAME + ".logd");
    os::File::unlink(STORAGE_DIR + "/" + STORAGE_NAME + ".index");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
