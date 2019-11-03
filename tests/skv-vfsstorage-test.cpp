#include <string>

#include <gtest/gtest.h>

#include <ondisk/Volume.hpp>
#include <os/File.hpp>
#include <vfs/Storage.hpp>
#include <util/String.hpp>
#include <util/StringPath.hpp>
#include <util/Log.hpp>

using namespace skv;
using namespace skv::vfs;
using namespace skv::util;

class VFSStorageTest: public ::testing::Test {
#ifdef BUILDING_UNIX
    const std::string VOLUME_DIR  = "/tmp";
    const std::string VOLUME_N1_NAME = "volume1";
    const std::string VOLUME_N2_NAME = "volume2";
#else
#endif
protected:
    void SetUp() override {
        removeFiles();

        volume1_ = ondisk::make_ondisk_volume();
        volume2_ = ondisk::make_ondisk_volume();

        ASSERT_NE(volume1_, nullptr);
        ASSERT_NE(volume2_, nullptr);

        ASSERT_TRUE(volume1_->initialize(VOLUME_DIR, VOLUME_N1_NAME).isOk());
        ASSERT_TRUE(volume2_->initialize(VOLUME_DIR, VOLUME_N2_NAME).isOk());
    }

    void TearDown() override {
        ASSERT_TRUE(volume2_->deinitialize().isOk());
        ASSERT_TRUE(volume1_->deinitialize().isOk());

        volume2_.reset();
        volume1_.reset();

        removeFiles();
    }

    void removeFiles() {
        os::File::unlink(VOLUME_DIR + "/" + VOLUME_N1_NAME + ".logd");
        os::File::unlink(VOLUME_DIR + "/" + VOLUME_N1_NAME+ ".index");
        os::File::unlink(VOLUME_DIR + "/" + VOLUME_N2_NAME + ".logd");
        os::File::unlink(VOLUME_DIR + "/" + VOLUME_N2_NAME+ ".index");
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
        createPath(volume1_, "/a/b/c/d/e");
        createPath(volume2_, "/f/g/h/i/j");

        ASSERT_TRUE(storage_.mount(volume1_, "/", "/").isOk());
        ASSERT_TRUE(storage_.mount(volume2_, "/", "/").isOk());

        ASSERT_TRUE(storage_.mount(volume1_, "/a", "/volume1_a").isOk());
        ASSERT_TRUE(storage_.mount(volume2_, "/f", "/volume2_f").isOk());

        ASSERT_TRUE(storage_.mount(volume1_, "/a/b/c", "/volume1_c").isOk());
        ASSERT_TRUE(storage_.mount(volume2_, "/f/g/h", "/volume2_h").isOk());
    }

    void doUnmounts() {
        ASSERT_TRUE(storage_.unmount(volume1_, "/a/b/c", "/volume1_c").isOk());
        ASSERT_TRUE(storage_.unmount(volume2_, "/f/g/h", "/volume2_h").isOk());

        ASSERT_TRUE(storage_.unmount(volume1_, "/a", "/volume1_a").isOk());
        ASSERT_TRUE(storage_.unmount(volume2_, "/f", "/volume2_f").isOk());

        ASSERT_TRUE(storage_.unmount(volume1_, "/", "/").isOk());
        ASSERT_TRUE(storage_.unmount(volume2_, "/", "/").isOk());
    }

    IVolumePtr volume1_;
    IVolumePtr volume2_;
    vfs::Storage storage_;
};

TEST_F(VFSStorageTest, MountUnmoutTest) {
    doMounts();

    ASSERT_FALSE(storage_.mount(volume1_, "/", "/").isOk());
    ASSERT_FALSE(storage_.mount(volume2_, "/", "/").isOk());

    doUnmounts();

    ASSERT_FALSE(storage_.unmount(volume1_, "/", "/").isOk());
    ASSERT_FALSE(storage_.unmount(volume2_, "/", "/").isOk());
}

TEST_F(VFSStorageTest, OpenCloseTest) {
    doMounts();

    std::vector<std::string> openPaths = {"/",
                                          "/volume1_a/b",   "/volume2_f/g",
                                          "/volume1_c",     "/volume1_c/d/e",
                                          "/volume2_f",     "/volume2_f/g"};

    for (const auto& p : openPaths) {
        auto [status, handle] = storage_.open(p);

        ASSERT_TRUE(status.isOk());
        ASSERT_TRUE(storage_.close(handle).isOk());
    }

    doUnmounts();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}



