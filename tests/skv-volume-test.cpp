#include <sstream>

#include <gtest/gtest.h>

#include <ondisk/Volume.hpp>
#include <os/File.hpp>

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

        {
            auto [status, devHandle] = volume.link(rootHandle, "dev");
            ASSERT_TRUE(status.isOk());
        }
        {
            auto [status, procHandle] = volume.link(rootHandle, "proc");
            ASSERT_TRUE(status.isOk());

            {
                auto [status, selfHandle] = volume.link(procHandle, "self");
                ASSERT_TRUE(status.isOk());
            }
        }
        {
            auto [status, selfHandle] = volume.open("/proc/self");

            ASSERT_TRUE(status.isOk());
        }
        {
            auto [status, selfHandle] = volume.open("/proc/self"); // now with cache hit

            ASSERT_TRUE(status.isOk());
        }
    }

    ASSERT_TRUE(volume.deinitialize().isOk());
    ASSERT_FALSE(volume.initialized());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}




