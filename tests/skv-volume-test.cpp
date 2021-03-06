#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

#include <gtest/gtest.h>

#include <ondisk/Volume.hpp>
#include <os/File.hpp>
#include <util/Log.hpp>
#include <util/Unused.hpp>

using namespace skv;
using namespace skv::ondisk;

namespace {
#ifdef BUILDING_UNIX
    const std::string STORAGE_DIR  = "/tmp";
    const std::string STORAGE_NAME = "test_storage";
#else
    const std::string STORAGE_DIR  = ".";
    const std::string STORAGE_NAME = "test_storage";
#endif
    std::atomic<bool> go1{false}, go2{false};

    constexpr size_t NRUNS = 10000;
}

void mtTestRoutineN1(std::reference_wrapper<Volume> v) {
    static constexpr std::hash<std::thread::id> hasher;

    while (!go1.load(std::memory_order_acquire))
        std::this_thread::yield();

    Volume &volume = v;

	auto root = volume.entry("/");

    if (!root) {
        return;
    }

	auto id = std::to_string(hasher(std::this_thread::get_id()));

	SKV_UNUSED(volume.link(*root, id).isOk());

	auto path = "/" + id;
	auto handle = volume.entry(path);

	if (!handle)
		return;

	for (size_t i = 0; i < NRUNS; ++i) {
		if (i % 7 == 0)
			SKV_UNUSED(handle->setProperty(std::to_string(i), Property{ double(i) }));
		else if (i % 5 == 0) {
			SKV_UNUSED(handle->setProperty(std::to_string(i), Property{ std::to_string(i) }));
		}
		else if (i % 3 == 0) {
			SKV_UNUSED(handle->setProperty(std::to_string(i), Property{ float(i) }));
		}
		else
			SKV_UNUSED(handle->setProperty(std::to_string(i), Property{ "fizz buzz" }));
	}
}

TEST(VolumeTest, MTTestN1) {
    using namespace std::chrono;
    using namespace std::literals;

    Status status;
    Volume volume{status};

    ASSERT_TRUE(status.isOk());

    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".logd"));
    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".index"));

    status = volume.initialize(STORAGE_DIR, STORAGE_NAME);

    ASSERT_TRUE(status.isOk());
    ASSERT_TRUE(volume.initialized());

    std::vector<std::thread> threads;
    std::generate_n(std::back_inserter(threads),
                    2 * std::thread::hardware_concurrency(),
                    [&] { return std::thread{&mtTestRoutineN1, std::ref(volume)}; });

    std::this_thread::sleep_for(100ms); // ensure all other threads is started

    auto startTime = system_clock::now();

    go1.store(true, std::memory_order_release);

    std::for_each(std::begin(threads), std::end(threads),
                  [](auto& t) {
                      if (t.joinable())
                          t.join();
                      });

    auto endTime = system_clock::now();

    Log::i("MTTest", "Test #1 has took ", duration_cast<milliseconds>(endTime - startTime).count(), " ms");

    ASSERT_TRUE(volume.deinitialize().isOk());
    ASSERT_FALSE(volume.initialized());

    std::this_thread::sleep_for(20ms);

    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".logd"));
    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".index"));
}

void mtTestRoutineN2(std::reference_wrapper<Volume> v) {
    static constexpr std::hash<std::thread::id> hasher;

    while (!go2.load(std::memory_order_acquire))
        std::this_thread::yield();

    Volume &volume = v;

	auto root = volume.entry("/");

	if (!root)
		return;

	auto id = std::to_string(hasher(std::this_thread::get_id()));
	auto path = "/test";
	auto handle = volume.entry(path);

	if (!handle)
		return;

	for (size_t i = 0; i < NRUNS; ++i) {
		if (i % 7 == 0)
			SKV_UNUSED(handle->setProperty(id + std::to_string(i), Property{ double(i) }));
		else if (i % 5 == 0) {
			SKV_UNUSED(handle->setProperty(id + std::to_string(i), Property{ std::to_string(i) }));
		}
		else if (i % 3 == 0) {
			SKV_UNUSED(handle->setProperty(id + std::to_string(i), Property{ float(i) }));
		}
		else
			SKV_UNUSED(handle->setProperty(id + std::to_string(i), Property{ "fizz buzz" }));
	}
}

TEST(VolumeTest, MTTestN2) {
    using namespace std::chrono;
    using namespace std::literals;

    Status createStatus;
    Volume volume{createStatus};

    ASSERT_TRUE(createStatus.isOk());

    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".logd"));
    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".index"));

    auto opened = volume.initialize(STORAGE_DIR, STORAGE_NAME);

    ASSERT_TRUE(opened.isOk());
    ASSERT_TRUE(volume.initialized());

	auto root = volume.entry("/");

    ASSERT_TRUE(root != nullptr);

    ASSERT_TRUE(volume.link(*root, "test").isOk());

    std::vector<std::thread> threads;
    std::generate_n(std::back_inserter(threads),
                    2 * std::thread::hardware_concurrency(),
                    [&] { return std::thread{&mtTestRoutineN2, std::ref(volume)}; });

    std::this_thread::sleep_for(100ms); // ensure all other threads is started

    auto startTime = system_clock::now();

    go2.store(true, std::memory_order_release);

    std::for_each(std::begin(threads), std::end(threads),
                  [](auto&& t) {
                      if (t.joinable())
                          t.join();
                      });

    auto endTime = system_clock::now();

    Log::i("MTTest", "Test #2 has took ", duration_cast<milliseconds>(endTime - startTime).count(), " ms");

	auto handle = volume.entry("/test");
	ASSERT_TRUE(handle != nullptr);

    const auto& [status, properties] = handle->properties();
    ASSERT_TRUE(status.isOk());
	ASSERT_EQ(properties.size(), NRUNS * threads.size());

    ASSERT_TRUE(volume.deinitialize().isOk());
    ASSERT_FALSE(volume.initialized());

    std::this_thread::sleep_for(20ms);

    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".logd"));
    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".index"));
}

TEST(VolumeTest, OpenCloseLinkClaim) {
    Status status;
    Volume volume{status};

    ASSERT_TRUE(status.isOk());

    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".logd"));
    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".index"));

    status = volume.initialize(STORAGE_DIR, STORAGE_NAME);

    ASSERT_TRUE(status.isOk());
    ASSERT_TRUE(volume.initialized());

    ASSERT_FALSE(volume.release(nullptr).isOk());
    Volume::Token token = &volume;

    ASSERT_TRUE(volume.claim(token).isOk());
    ASSERT_FALSE(volume.claim(Volume::Token{}).isOk());
    ASSERT_TRUE(volume.release(token).isOk());
    ASSERT_FALSE(volume.release(token).isOk());
    ASSERT_FALSE(volume.claim(Volume::Token{}).isOk());
    ASSERT_FALSE(volume.release(Volume::Token{}).isOk());

    {
        auto root = volume.entry("/");

        ASSERT_TRUE(root != nullptr);

        status = volume.link(*root, "dev");
        ASSERT_TRUE(status.isOk());

        status = volume.link(*root, "proc");
        ASSERT_TRUE(status.isOk());

        status = volume.link(*root, "usr");
        ASSERT_TRUE(status.isOk());

        std::set<std::string> children;
        std::tie(status, children) = root->links();
        ASSERT_TRUE(status.isOk());
		ASSERT_EQ(children.size(), 3);
    }

    {
        auto proc = volume.entry("/proc");

        ASSERT_TRUE(proc != nullptr);

        status = volume.link(*proc, "1");
        ASSERT_TRUE(status.isOk());

        status = volume.link(*proc, "2");
        ASSERT_TRUE(status.isOk());

        std::set<std::string> children;
        std::tie(status, children) = proc->links();
        ASSERT_TRUE(status.isOk());
		ASSERT_EQ(children.size(), 2);
    }

    {
		auto proc = volume.entry("/proc");

		ASSERT_TRUE(proc != nullptr);

        std::set<std::string> children;
        std::tie(status, children) = proc->links();
        ASSERT_TRUE(status.isOk());
		ASSERT_EQ(children.size(), 2);

        status = volume.link(*proc, "self");
        ASSERT_TRUE(status.isOk());

        status = volume.link(*proc, "self");
        ASSERT_FALSE(status.isOk());
    }

    {
		auto self = volume.entry("/proc/self");

		ASSERT_TRUE(self != nullptr);

        std::set<std::string> children;
        std::tie(status, children) = self->links();
        ASSERT_TRUE(status.isOk());
		ASSERT_TRUE(children.empty());

        ASSERT_TRUE(self->setProperty("int_property", 1).isOk());
        ASSERT_TRUE(self->setProperty("str_property", "some text").isOk());
        ASSERT_TRUE(self->setProperty("flt_property", 1024.0f).isOk());
        ASSERT_TRUE(self->setProperty("dbl_property", 4096.0).isOk());
    }

    {
		auto self = volume.entry("/proc/self");

		ASSERT_TRUE(self != nullptr);

        std::set<std::string> children;
        std::tie(status, children) = self->links();
        ASSERT_TRUE(status.isOk());
		ASSERT_TRUE(children.empty());

        bool v = false;

        std::tie(status, v) = self->hasProperty("int_property");
        ASSERT_TRUE(status.isOk());
        ASSERT_TRUE(v);

        std::tie(status, v) = self->hasProperty("str_property");
        ASSERT_TRUE(status.isOk());
        ASSERT_TRUE(v);

        std::tie(status, v) = self->hasProperty("flt_property");
        ASSERT_TRUE(status.isOk());
        ASSERT_TRUE(v);

        std::tie(status, v) = self->hasProperty("dbl_property");
        ASSERT_TRUE(status.isOk());
        ASSERT_TRUE(v);
    }

    {
		auto self = volume.entry("/proc/self");

		ASSERT_TRUE(self != nullptr);

        auto [s1, v1] = self->property("int_property");
        auto [s2, v2] = self->property("str_property");
        auto [s3, v3] = self->property("flt_property");
        auto [s4, v4] = self->property("dbl_property");

        ASSERT_TRUE(s1.isOk() && v1 == Property{1});
        ASSERT_TRUE(s2.isOk() && v2 == Property{"some text"});
        ASSERT_TRUE(s3.isOk() && v3 == Property{1024.0f});
        ASSERT_TRUE(s4.isOk() && v4 == Property{4096.0});
    }

    ASSERT_TRUE(volume.deinitialize().isOk());
    ASSERT_FALSE(volume.initialized());

    status = volume.initialize(STORAGE_DIR, STORAGE_NAME);

    ASSERT_TRUE(status.isOk());
    ASSERT_TRUE(volume.initialized());

    ASSERT_TRUE(volume.deinitialize().isOk());
    ASSERT_FALSE(volume.initialized());

    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".logd"));
    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".index"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
