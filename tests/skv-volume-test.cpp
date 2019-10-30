#include <sstream>

#include <gtest/gtest.h>

#include <ondisk/Volume.hpp>

using namespace skv::ondisk;

namespace {
#ifdef BUILDING_UNIX
    const char * const STORAGE_DIR  = "/tmp";
    const char * const STORAGE_NAME = "test_storage";
#else
    const char * const STORAGE_DIR  = ".";
    const char * const STORAGE_NAME = "test_storage";
#endif
}

TEST(VolumeTest, OpenCloseLink) {
    Volume volume;

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
        }
    }

    ASSERT_TRUE(volume.deinitialize().isOk());
    ASSERT_FALSE(volume.initialized());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}




