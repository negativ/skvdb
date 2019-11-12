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
        SKV_UNUSED(os::File::unlink(VOLUME_DIR + os::File::sep() + VOLUME_N1_NAME + ".logd"));
        SKV_UNUSED(os::File::unlink(VOLUME_DIR + os::File::sep() + VOLUME_N1_NAME+ ".index"));
        SKV_UNUSED(os::File::unlink(VOLUME_DIR + os::File::sep() + VOLUME_N2_NAME + ".logd"));
        SKV_UNUSED(os::File::unlink(VOLUME_DIR + os::File::sep() + VOLUME_N2_NAME+ ".index"));
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

TEST_F(VFSStorageTest, PropertiesGetSetRemoveTest) {
    doMounts();

    {
        auto [status, handle] = volume1_->open("/a/b/c/d");

        SKV_UNUSED(status);

        ASSERT_TRUE(volume1_->setProperty(handle, "test_int", Property{1024 * 1024}).isOk());
        ASSERT_TRUE(volume1_->setProperty(handle, "test_str", Property{"First test text"}).isOk());
        ASSERT_TRUE(volume1_->setProperty(handle, "test_flt", Property{1.0f}).isOk());
        ASSERT_TRUE(volume1_->setProperty(handle, "test_dbl", Property{123.0}).isOk());
        ASSERT_TRUE(volume2_->setProperty(handle, "v1_test_dbl", Property{128.32}).isOk());

        ASSERT_TRUE(volume1_->close(handle).isOk());
    }

    {
        auto [status, handle] = volume2_->open("/f/g/h/i");
        SKV_UNUSED(status);
        ASSERT_TRUE(volume2_->setProperty(handle, "test_int", Property{1024 * 1024 * 1024}).isOk());
        ASSERT_TRUE(volume2_->setProperty(handle, "test_str", Property{"Second test text"}).isOk());
        ASSERT_TRUE(volume2_->setProperty(handle, "test_flt", Property{100.0f}).isOk());
        ASSERT_TRUE(volume2_->setProperty(handle, "test_dbl", Property{0.123}).isOk());
        ASSERT_TRUE(volume2_->setProperty(handle, "v2_test_dbl", Property{64.1}).isOk());

        ASSERT_TRUE(volume2_->close(handle).isOk());
    }

    {
        auto [status, handle] = storage_.open("/combined");
        SKV_UNUSED(status);
        {
            {
                auto [status, v] = storage_.hasProperty(handle, "test_int");
                ASSERT_TRUE(status.isOk());
                ASSERT_TRUE(v);
            }
            {
                auto [status, v] = storage_.hasProperty(handle, "test_dbl");
                ASSERT_TRUE(status.isOk());
                ASSERT_TRUE(v);
            }
            {
                auto [status, v] = storage_.hasProperty(handle, "v2_test_dbl");
                ASSERT_TRUE(status.isOk());
                ASSERT_TRUE(v);
            }
        }

        {
            auto [status, properties] = storage_.properties(handle);
            SKV_UNUSED(status);
            ASSERT_EQ(properties.size(), 6);

            ASSERT_EQ(properties["test_int"], Property{1024 * 1024 * 1024}); // volume 2 has highest priority
            ASSERT_EQ(properties["test_str"], Property{"Second test text"});
            ASSERT_EQ(properties["test_flt"], Property{100.0f});
            ASSERT_EQ(properties["test_dbl"], Property{0.123});
            ASSERT_EQ(properties["v1_test_dbl"], Property{128.32});
            ASSERT_EQ(properties["v2_test_dbl"], Property{64.1});

            {
                auto [status, value] = storage_.property(handle, "test_int");
                ASSERT_TRUE(status.isOk());
                ASSERT_EQ(value, Property{1024 * 1024 * 1024});
            }
            {
                auto [status, value] = storage_.property(handle, "test_str");
                ASSERT_TRUE(status.isOk());
                ASSERT_EQ(value, Property{"Second test text"});
            }
            {
                auto [status, value] = storage_.property(handle, "test_flt");
                ASSERT_TRUE(status.isOk());
                ASSERT_EQ(value, Property{100.0f});
            }
            {
                auto [status, value] = storage_.property(handle, "test_dbl");
                ASSERT_TRUE(status.isOk());
                ASSERT_EQ(value, Property{0.123});
            }
            {
                auto [status, value] = storage_.property(handle, "v1_test_dbl");
                ASSERT_TRUE(status.isOk());
                ASSERT_EQ(value, Property{128.32});
            }
            {
                auto [status, value] = storage_.property(handle, "v2_test_dbl");
                ASSERT_TRUE(status.isOk());
                ASSERT_EQ(value, Property{64.1});
            }
        }

        ASSERT_TRUE(storage_.setProperty(handle, "combined_property", Property{std::string(1024, 'a')}).isOk());

        {
            auto [status, handle] = volume1_->open("/a/b/c/d");
            auto [pstatus, v] = volume1_->hasProperty(handle, "combined_property");
            SKV_UNUSED(status);
            ASSERT_TRUE(pstatus.isOk());
            ASSERT_TRUE(v);

            {
                auto [status, value] = volume1_->property(handle, "combined_property");

                ASSERT_TRUE(status.isOk());
                ASSERT_EQ(value, Property{std::string(1024, 'a')});
            }

            ASSERT_TRUE(volume1_->close(handle).isOk());
        }
        {
            auto [status, handle] = volume2_->open("/f/g/h/i");
            auto [pstatus, v] = volume2_->hasProperty(handle, "combined_property");
            SKV_UNUSED(status);
            ASSERT_TRUE(pstatus.isOk());
            ASSERT_TRUE(v);

            {
                auto [status, value] = volume2_->property(handle, "combined_property");

                ASSERT_TRUE(status.isOk());
                ASSERT_EQ(value, Property{std::string(1024, 'a')});
            }

            ASSERT_TRUE(volume2_->close(handle).isOk());
        }

        ASSERT_TRUE(storage_.close(handle).isOk());
    }

    {
        auto [status, handle] = storage_.open("/combined");
        SKV_UNUSED(status);
        ASSERT_TRUE(storage_.removeProperty(handle, "test_int").isOk());
        ASSERT_TRUE(storage_.removeProperty(handle, "test_str").isOk());
        ASSERT_TRUE(storage_.removeProperty(handle, "test_flt").isOk());
        ASSERT_TRUE(storage_.removeProperty(handle, "test_dbl").isOk());
        ASSERT_TRUE(storage_.removeProperty(handle, "v1_test_dbl").isOk());
        ASSERT_TRUE(storage_.removeProperty(handle, "v2_test_dbl").isOk());
        ASSERT_TRUE(storage_.removeProperty(handle, "combined_property").isOk());

        {
            auto [status, handle] = volume1_->open("/a/b/c/d");
            auto [pstatus, properties] = volume1_->properties(handle);

            SKV_UNUSED(status);
            ASSERT_TRUE(pstatus.isOk());
            ASSERT_TRUE(properties.empty());

            ASSERT_TRUE(volume1_->close(handle).isOk());
        }

        {
            auto [status, handle] = volume2_->open("/f/g/h/i");
            auto [pstatus, properties] = volume2_->properties(handle);

            SKV_UNUSED(status);
            ASSERT_TRUE(pstatus.isOk());
            ASSERT_TRUE(properties.empty());

            ASSERT_TRUE(volume2_->close(handle).isOk());
        }

        ASSERT_TRUE(storage_.close(handle).isOk());
    }

    doUnmounts();
}

TEST_F(VFSStorageTest, PropertyExpireTest) {
    using namespace std::chrono;
    using namespace std::literals;

    doMounts();

    auto [status, handle] = storage_.open("/combined");

    SKV_UNUSED(status);
    ASSERT_TRUE(storage_.setProperty(handle, "test_int", Property{1024 * 1024}).isOk());
    ASSERT_TRUE(storage_.setProperty(handle, "test_str", Property{"First test text"}).isOk());
    ASSERT_TRUE(storage_.setProperty(handle, "test_flt", Property{1.0f}).isOk());
    ASSERT_TRUE(storage_.setProperty(handle, "test_dbl", Property{123.0}).isOk());

    ASSERT_TRUE(storage_.expireProperty(handle, "test_int", Storage::Clock::now() + 100ms).isOk());
    ASSERT_TRUE(storage_.expireProperty(handle, "test_str", Storage::Clock::now() + 200ms).isOk());
    ASSERT_TRUE(storage_.expireProperty(handle, "test_flt", Storage::Clock::now() + 300ms).isOk());
    ASSERT_TRUE(storage_.expireProperty(handle, "test_dbl", Storage::Clock::now() + 400ms).isOk());

    std::this_thread::sleep_for(150ms);

    {
        auto [pstatus, v] = volume2_->hasProperty(handle, "test_int");

        ASSERT_TRUE(pstatus.isOk());
        ASSERT_FALSE(v);
    }

    std::this_thread::sleep_for(100ms);

    {
        auto [pstatus, v] = volume2_->hasProperty(handle, "test_str");

        ASSERT_TRUE(pstatus.isOk());
        ASSERT_FALSE(v);
    }

    std::this_thread::sleep_for(100ms);

    {
        auto [pstatus, v] = volume2_->hasProperty(handle, "test_flt");

        ASSERT_TRUE(pstatus.isOk());
        ASSERT_FALSE(v);
    }

    std::this_thread::sleep_for(100ms);

    {
        auto [pstatus, v] = volume2_->hasProperty(handle, "test_dbl");

        ASSERT_TRUE(pstatus.isOk());
        ASSERT_FALSE(v);
    }

    {
        auto [status, handle] = volume1_->open("/a/b/c/d");
        auto [pstatus, properties] = volume1_->properties(handle);

        SKV_UNUSED(status);
        ASSERT_TRUE(pstatus.isOk());
        ASSERT_TRUE(properties.empty());

        ASSERT_TRUE(volume1_->close(handle).isOk());
    }

    {
        auto [status, handle] = volume2_->open("/f/g/h/i");
        auto [pstatus, properties] = volume2_->properties(handle);

        SKV_UNUSED(status);
        ASSERT_TRUE(pstatus.isOk());
        ASSERT_TRUE(properties.empty());

        ASSERT_TRUE(volume2_->close(handle).isOk());
    }

    ASSERT_TRUE(storage_.close(handle).isOk());

    doUnmounts();
}

TEST_F(VFSStorageTest, LinkUnlinkTest) {
    doMounts();

    auto [unused, handle] = storage_.open("/combined");
    SKV_UNUSED(unused);
    {
        auto [status, links] = storage_.links(handle);

        ASSERT_TRUE(status.isOk());
        ASSERT_FALSE(links.empty());

        ASSERT_EQ(links.size(), 2);

        ASSERT_TRUE(links.find("e") != std::cend(links));
        ASSERT_TRUE(links.find("j") != std::cend(links));
    }

    ASSERT_TRUE(storage_.link(handle,  "w").isOk());
    ASSERT_FALSE(storage_.link(handle, "w").isOk());
    ASSERT_TRUE(storage_.link(handle,  "x").isOk());
    ASSERT_FALSE(storage_.link(handle, "x").isOk());

    {
        auto [status, links] = storage_.links(handle);

        ASSERT_TRUE(status.isOk());
        ASSERT_FALSE(links.empty());

        ASSERT_EQ(links.size(), 4);

        ASSERT_TRUE(links.find("e") != std::cend(links));
        ASSERT_TRUE(links.find("j") != std::cend(links));
        ASSERT_TRUE(links.find("w") != std::cend(links));
        ASSERT_TRUE(links.find("x") != std::cend(links));
    }

    ASSERT_TRUE(storage_.unlink(handle, "e").isOk());
    ASSERT_FALSE(storage_.unlink(handle, "e").isOk());

    {
        auto [status, links] = storage_.links(handle);

        ASSERT_TRUE(status.isOk());
        ASSERT_FALSE(links.empty());

        ASSERT_EQ(links.size(), 3);

        ASSERT_FALSE(links.find("e") != std::cend(links));
        ASSERT_TRUE(links.find("j") != std::cend(links));
        ASSERT_TRUE(links.find("w") != std::cend(links));
        ASSERT_TRUE(links.find("x") != std::cend(links));
    }

    ASSERT_TRUE(storage_.close(handle).isOk());

    doUnmounts();
}

void setProps(IVolumePtr volume, std::string_view path, std::initializer_list<std::pair<std::string_view, Property>> props) {
    auto [status, handle] = volume->open(path);

    if (!status.isOk) {
        std::cerr << "Error opening: " << status.message() << std::endl;

        return;
    }

    for (const auto& [name, value] : props) {
        auto status = volume->setProperty(handle, name, value);

        if (!status.isOk()) {
            std::cerr << "setProeprty error: " << status.message() << std::endl;
        }
    }

    if (!volume->close(handle).isOk()) {
        std::cerr << "Error closing: " << status.message() << std::endl;
    }
}

TEST(SomeTest, Example) {
    using namespace std::literals;

    vfs::Storage storage;
    IVolumePtr vol1 = skv::ondisk::make_ondisk_volume();
    IVolumePtr vol2 = skv::ondisk::make_ondisk_volume();

    if (!(vol1->initialize(".", "volume1").isOk() &&
        vol2->initialize(".", "volume2").isOk())) {
        std::cerr << "Unable to initialize ondisk volumes!" << std::endl;
    }

    storage.mount(vol1, "/", "/");                                      // volume #1 become root item for VFS
    storage.mount(vol1, "/", "/combined", Storage::DefaultPriority); // volume #1 & #2 would be accessible via /combined path
    storage.mount(vol2, "/", "/combined", Storage::MinPriority);     // volume 2 has minimal priority

    {   // You can work directly with volume #1
        auto [status, handle] = vol1->open("/");

        vol1->setProperty(handle, "shared_property", vfs::Property{ std::vector<char>(1024, 'A') });
        vol1->setProperty(handle, "int_property", vfs::Property{ 123 });
        vol1->setProperty(handle, "dbl_property", vfs::Property{ 873.0 });

        vol1->link(handle, "volume1child"); // creating new child with name volume1child

        vol1->close(handle);
    }

    {   // You can work directly with volume #2
        auto [status, handle] = vol2->open("/");

        vol2->setProperty(handle, "shared_property", vfs::Property{ std::vector<char>(1024, 'B') }); // this property would be shadowed in VFS by volume #1
                                                                                                   // as volume 2 has minimal priority
        vol2->setProperty(handle, "int_property", vfs::Property{ 123 });
        vol2->setProperty(handle, "flt_property", vfs::Property{ 512.0F });

        vol2->link(handle, "volume2child"); // creating new child with name volume2child

        vol2->close(handle);
    }

    auto [status, handle] = storage.open("/combined");

    if (status.isOk()) {
        storage.setProperty(handle, "some_text_prop", vfs::Property{ "Some text here" });
        storage.setProperty(handle, "int_property", vfs::Property{ 1000 });
        storage.setProperty(handle, "another_text_property", vfs::Property{ "text from VFS" });

        auto [status, properties] = storage.properties(handle);

        if (status.isOk()) {
            std::cout << "/combined has properties: " << std::endl;

            for (const auto& [name, value] : properties) {
                std::cout << "\t> " << name << std::endl;
                // Do something with each property/value
            }
        }

        if (auto [status, exist] = storage.hasProperty(handle, "not_existing"); status.isOk() && !exist) {
            std::cout << "Property \"not_existing\" doesn\'t exists" << std::endl;
        }

        if (auto [status, value] = storage.property(handle, "shared_property"); status.isOk()) {
            if (value == vfs::Property{ std::vector<char>(1024, 'A') })
                std::cout << "Property \"shared_property\" full of 'A's" << std::endl;
            else if (value == vfs::Property{ std::vector<char>(1024, 'B') })
                std::cout << "Property \"shared_property\" full of 'B's" << std::endl;
            else
                std::cout << "Hmmmm.... Impossible" << std::endl;
        }
        else {
            std::cout << "Property \"shared_property\" doesn\'t exists" << std::endl;
        }

        if (auto [status, links] = storage.links(handle); status.isOk()) {
            std::cout << "/combined has links: " << std::endl;

            for (const auto& l : links) {
                std::cout << "\t> " << l << std::endl;
                // Do something with each property/value
            }
        }

        storage.expireProperty(handle, "some_text_prop", Storage::Clock::now() + 100ms);

        std::this_thread::sleep_for(200ms);

        storage.removeProperty(handle, "int_property");

        storage.close(handle);
    }

    {
        auto [status, handle] = vol1->open("/");

        if (auto [status, properties] = vol1->properties(handle); status.isOk()) {
            std::cout << "volume's #1 '/' has properties: " << std::endl;

            for (const auto& [name, value] : properties) {
                std::cout << "\t> " << name << std::endl;
                // Do something with each property/value
            }
        }

        vol1->close(handle);
    }

    {
        auto [status, handle] = vol2->open("/");

        vol2->removeProperty(handle, "shared_property");

        if (auto [status, properties] = vol2->properties(handle); status.isOk()) {
            std::cout << "volume's #2 '/' has properties: " << std::endl;

            for (const auto& [name, value] : properties) {
                std::cout << "\t> " << name << std::endl;
                // Do something with each property/value
            }
        }

        vol2->close(handle);
    }

    storage.unmount(vol2, "/", "/combined");
    storage.unmount(vol1, "/", "/combined");
    storage.unmount(vol1, "/", "/");

    vol2->deinitialize();
    vol1->deinitialize();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
