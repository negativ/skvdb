#include <string>
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

class VFSStorageTest: public ::testing::Test {
#ifdef BUILDING_UNIX
    const std::string VOLUME_DIR  = "/tmp";
    const std::string VOLUME_N1_NAME = "volume1";
    const std::string VOLUME_N2_NAME = "volume2";
#else
    const std::string VOLUME_DIR  = ".";
    const std::string VOLUME_N1_NAME = "volume1";
    const std::string VOLUME_N2_NAME = "volume2";
#endif
protected:
    void SetUp() override {
        ASSERT_TRUE(storageStatus.isOk());

        removeFiles();

        Status status;
        volume1_ = std::make_shared<ondisk::Volume>(status);

        ASSERT_TRUE(status.isOk());

        volume2_ = std::make_shared<ondisk::Volume>(status);

        ASSERT_TRUE(status.isOk());

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
        SKV_UNUSED(os::File::unlink(VOLUME_DIR + char(os::path::separator) + VOLUME_N1_NAME + ".logd"));
        SKV_UNUSED(os::File::unlink(VOLUME_DIR + char(os::path::separator) + VOLUME_N1_NAME+ ".index"));
        SKV_UNUSED(os::File::unlink(VOLUME_DIR + char(os::path::separator) + VOLUME_N2_NAME + ".logd"));
        SKV_UNUSED(os::File::unlink(VOLUME_DIR + char(os::path::separator) + VOLUME_N2_NAME+ ".index"));
    }

    void createPath(std::shared_ptr<ondisk::Volume>& ptr, const std::string& path) {
        auto root = ptr->entry("/");

        ASSERT_NE(root, nullptr);

        const auto& tokens = util::split(util::simplifyPath(path), '/');
        std::string trackPath = "";

        for (const auto& token : tokens) {
            auto [status, children] = root->links();

            ASSERT_TRUE(status.isOk());

            trackPath += ("/" + token);

            if (auto it = std::find(std::cbegin(children), std::cend(children), token); it != std::cend(children)) {
                auto handle = ptr->entry(trackPath);

                ASSERT_NE(handle, nullptr);

                std::swap(root, handle);

                continue;
            }
            else {
                auto status = ptr->link(*root, token);

                ASSERT_TRUE(status.isOk());

                auto handle = ptr->entry(trackPath);

                ASSERT_NE(handle, nullptr);

                std::swap(root, handle);
            }
        }
    }

    void doMounts() {
        createPath(volume1_, "/a/b/c/d/e");
        createPath(volume2_, "/f/g/h/i/j");

        ASSERT_TRUE(storage_.mount(volume1_, "/", "/").isOk());
        ASSERT_TRUE(storage_.mount(volume2_, "/", "/").isOk());

        ASSERT_TRUE(storage_.mount(volume1_, "/a", "/volume1_a", Storage::MaxPriority).isOk());
        ASSERT_TRUE(storage_.mount(volume2_, "/f", "/volume2_f").isOk());

        ASSERT_TRUE(storage_.mount(volume1_, "/a/b/c", "/volume1_c").isOk());
        ASSERT_TRUE(storage_.mount(volume2_, "/f/g/h", "/volume2_h", Storage::MaxPriority).isOk());

        ASSERT_TRUE(storage_.mount(volume1_, "/a/b/c/d", "/combined").isOk());
        ASSERT_TRUE(storage_.mount(volume2_, "/f/g/h/i", "/combined", Storage::MaxPriority).isOk());
    }

    void doUnmounts() {
        ASSERT_TRUE(storage_.unmount(volume1_, "/a/b/c", "/volume1_c").isOk());
        ASSERT_TRUE(storage_.unmount(volume2_, "/f/g/h", "/volume2_h").isOk());

        ASSERT_TRUE(storage_.unmount(volume1_, "/a", "/volume1_a").isOk());
        ASSERT_TRUE(storage_.unmount(volume2_, "/f", "/volume2_f").isOk());

        ASSERT_TRUE(storage_.unmount(volume1_, "/", "/").isOk());
        ASSERT_TRUE(storage_.unmount(volume2_, "/", "/").isOk());

        ASSERT_TRUE(storage_.unmount(volume1_, "/a/b/c/d", "/combined").isOk());
        ASSERT_TRUE(storage_.unmount(volume2_, "/f/g/h/i", "/combined").isOk());
    }

    std::shared_ptr<ondisk::Volume> volume1_;
    std::shared_ptr<ondisk::Volume> volume2_;
    Status storageStatus;
    vfs::Storage storage_{storageStatus};
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
        auto handle = storage_.entry(p);

        ASSERT_NE(handle, nullptr);
    }

    doUnmounts();
}

TEST_F(VFSStorageTest, PropertiesGetSetRemoveTest) {
    doMounts();

    {
        auto handle = volume1_->entry("/a/b/c/d");

        ASSERT_NE(handle, nullptr);

        ASSERT_TRUE(handle->setProperty("test_int", Property{1024 * 1024}).isOk());
        ASSERT_TRUE(handle->setProperty("test_str", Property{"First test text"}).isOk());
        ASSERT_TRUE(handle->setProperty("test_flt", Property{1.0f}).isOk());
        ASSERT_TRUE(handle->setProperty("test_dbl", Property{123.0}).isOk());
        ASSERT_TRUE(handle->setProperty("v1_test_dbl", Property{128.32}).isOk());
    }

    {
        auto handle = volume2_->entry("/f/g/h/i");

        ASSERT_NE(handle, nullptr);

        ASSERT_TRUE(handle->setProperty("test_int", Property{1024 * 1024 * 1024}).isOk());
        ASSERT_TRUE(handle->setProperty("test_str", Property{"Second test text"}).isOk());
        ASSERT_TRUE(handle->setProperty("test_flt", Property{100.0f}).isOk());
        ASSERT_TRUE(handle->setProperty("test_dbl", Property{0.123}).isOk());
        ASSERT_TRUE(handle->setProperty("v2_test_dbl", Property{64.1}).isOk());
    }

    {
        auto handle = storage_.entry("/combined");
        bool v = false;
        Status status;

        ASSERT_NE(handle, nullptr);

        std::tie(status, v) = handle->hasProperty("test_int");
        ASSERT_TRUE(status.isOk());
        ASSERT_TRUE(v);

        std::tie(status, v) = handle->hasProperty("test_dbl");
        ASSERT_TRUE(status.isOk());
        ASSERT_TRUE(v);

        std::tie(status, v) = handle->hasProperty("v2_test_dbl");
        ASSERT_TRUE(status.isOk());
        ASSERT_TRUE(v);

        IEntry::Properties properties;
        std::tie(status, properties) = handle->properties();

        ASSERT_TRUE(status.isOk());
        ASSERT_EQ(properties.size(), 6);

        ASSERT_EQ(properties["test_int"], Property{1024 * 1024 * 1024}); // volume 2 has highest priority
        ASSERT_EQ(properties["test_str"], Property{"Second test text"});
        ASSERT_EQ(properties["test_flt"], Property{100.0f});
        ASSERT_EQ(properties["test_dbl"], Property{0.123});
        ASSERT_EQ(properties["v1_test_dbl"], Property{128.32});
        ASSERT_EQ(properties["v2_test_dbl"], Property{64.1});

        {
            auto [status, value] = handle->property("test_int");
            ASSERT_TRUE(status.isOk());
            ASSERT_EQ(value, Property{1024 * 1024 * 1024});
        }
        {
            auto [status, value] = handle->property("test_str");
            ASSERT_TRUE(status.isOk());
            ASSERT_EQ(value, Property{"Second test text"});
        }
        {
            auto [status, value] = handle->property("test_flt");
            ASSERT_TRUE(status.isOk());
            ASSERT_EQ(value, Property{100.0f});
        }
        {
            auto [status, value] = handle->property("test_dbl");
            ASSERT_TRUE(status.isOk());
            ASSERT_EQ(value, Property{0.123});
        }
        {
            auto [status, value] = handle->property("v1_test_dbl");
            ASSERT_TRUE(status.isOk());
            ASSERT_EQ(value, Property{128.32});
        }
        {
            auto [status, value] = handle->property("v2_test_dbl");
            ASSERT_TRUE(status.isOk());
            ASSERT_EQ(value, Property{64.1});
        }

        ASSERT_TRUE(handle->setProperty("combined_property", Property{std::string(1024, 'a')}).isOk());

        {
            auto handle = volume1_->entry("/a/b/c/d");
            ASSERT_NE(handle, nullptr);

			auto [status, v] = handle->hasProperty("combined_property");

			ASSERT_TRUE(status.isOk());
            ASSERT_TRUE(v);

            {
                auto [status, value] = handle->property("combined_property");

                ASSERT_TRUE(status.isOk());
                ASSERT_EQ(value, Property{std::string(1024, 'a')});
            }
        }
        {
            auto handle = volume2_->entry("/f/g/h/i");
            ASSERT_NE(handle, nullptr);

			auto [status, v] = handle->hasProperty("combined_property");

			ASSERT_TRUE(status.isOk());
            ASSERT_TRUE(v);
            {
                auto [status, value] = handle->property("combined_property");

                ASSERT_TRUE(status.isOk());
                ASSERT_EQ(value, Property{std::string(1024, 'a')});
            }
        }
    }

    {
        auto handle = storage_.entry("/combined");

        ASSERT_NE(handle, nullptr);

        ASSERT_TRUE(handle->removeProperty("test_int").isOk());
        ASSERT_TRUE(handle->removeProperty("test_str").isOk());
        ASSERT_TRUE(handle->removeProperty("test_flt").isOk());
        ASSERT_TRUE(handle->removeProperty("test_dbl").isOk());
        ASSERT_TRUE(handle->removeProperty("v1_test_dbl").isOk());
        ASSERT_TRUE(handle->removeProperty("v2_test_dbl").isOk());
        ASSERT_TRUE(handle->removeProperty("combined_property").isOk());

        {
            auto handle = volume1_->entry("/a/b/c/d");

            ASSERT_NE(handle, nullptr);

            auto [status, properties] = handle->properties();

            ASSERT_TRUE(status.isOk());
            ASSERT_TRUE(properties.empty());
        }

        {
            auto handle = volume2_->entry("/f/g/h/i");

            ASSERT_NE(handle, nullptr);

            auto [status, properties] = handle->properties();

            ASSERT_TRUE(status.isOk());
            ASSERT_TRUE(properties.empty());
        }
    }

    doUnmounts();
}

TEST_F(VFSStorageTest, PropertyExpireTest) {
    using namespace std::chrono;
    using namespace std::literals;

    doMounts();

    auto handle = storage_.entry("/combined");

    ASSERT_NE(handle, nullptr);

    ASSERT_TRUE(handle->setProperty("test_int", Property{1024 * 1024}).isOk());
    ASSERT_TRUE(handle->setProperty("test_str", Property{"First test text"}).isOk());
    ASSERT_TRUE(handle->setProperty("test_flt", Property{1.0f}).isOk());
    ASSERT_TRUE(handle->setProperty("test_dbl", Property{123.0}).isOk());

    ASSERT_TRUE(handle->expireProperty("test_int", 100ms).isOk());
    ASSERT_TRUE(handle->expireProperty("test_str", 200ms).isOk());
    ASSERT_TRUE(handle->expireProperty("test_flt", 300ms).isOk());
    ASSERT_TRUE(handle->expireProperty("test_dbl", 400ms).isOk());

    bool v = false;
    Status status;

    std::this_thread::sleep_for(150ms);

    std::tie(status, v) = handle->hasProperty("test_int");
    ASSERT_TRUE(status.isOk());
    ASSERT_FALSE(v);

    std::this_thread::sleep_for(100ms);

    std::tie(status, v) = handle->hasProperty("test_str");
    ASSERT_TRUE(status.isOk());
    ASSERT_FALSE(v);

    std::this_thread::sleep_for(100ms);

    std::tie(status, v) = handle->hasProperty("test_flt");
    ASSERT_TRUE(status.isOk());
    ASSERT_FALSE(v);

    std::this_thread::sleep_for(100ms);

    std::tie(status, v) = handle->hasProperty("test_dbl");
    ASSERT_TRUE(status.isOk());
    ASSERT_FALSE(v);

    {
        auto handle = volume1_->entry("/a/b/c/d");
        IEntry::Properties props;
        std::tie(status, props) = handle->properties();
        ASSERT_TRUE(props.empty());
    }

    {
        auto handle = volume2_->entry("/f/g/h/i");
        IEntry::Properties props;
        std::tie(status, props) = handle->properties();
        ASSERT_TRUE(props.empty());
    }

    doUnmounts();
}

TEST_F(VFSStorageTest, LinkUnlinkTest) {
    doMounts();

    auto handle = storage_.entry("/combined");
    ASSERT_NE(handle, nullptr);

    {
        auto [status, links] = handle->links();

        ASSERT_TRUE(status.isOk());
        ASSERT_FALSE(links.empty());

        ASSERT_EQ(links.size(), 2);

        ASSERT_TRUE(links.find("e") != std::cend(links));
        ASSERT_TRUE(links.find("j") != std::cend(links));
    }

    ASSERT_TRUE(storage_.link(*handle,  "w").isOk());
    ASSERT_FALSE(storage_.link(*handle, "w").isOk());
    ASSERT_TRUE(storage_.link(*handle,  "x").isOk());
    ASSERT_FALSE(storage_.link(*handle, "x").isOk());

    {
        auto [status, links] = handle->links();

        ASSERT_TRUE(status.isOk());
        ASSERT_FALSE(links.empty());

        ASSERT_EQ(links.size(), 4);

        ASSERT_TRUE(links.find("e") != std::cend(links));
        ASSERT_TRUE(links.find("j") != std::cend(links));
        ASSERT_TRUE(links.find("w") != std::cend(links));
        ASSERT_TRUE(links.find("x") != std::cend(links));
    }

    ASSERT_TRUE(storage_.unlink(*handle, "e").isOk());
    ASSERT_FALSE(storage_.unlink(*handle, "e").isOk());

    {
        auto [status, links] = handle->links();

        ASSERT_TRUE(status.isOk());
        ASSERT_FALSE(links.empty());

        ASSERT_EQ(links.size(), 3);

        ASSERT_FALSE(links.find("e") != std::cend(links));
        ASSERT_TRUE(links.find("j") != std::cend(links));
        ASSERT_TRUE(links.find("w") != std::cend(links));
        ASSERT_TRUE(links.find("x") != std::cend(links));
    }

    doUnmounts();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
