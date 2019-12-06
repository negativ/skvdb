#include <sstream>

#include <gtest/gtest.h>

#include <ondisk/StorageEngine.hpp>
#include <os/File.hpp>

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
}

TEST(StorageTest, OpenClose) {
    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".logd"));
    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".index"));
    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".logdc"));

    StorageEngine<> storage;

    auto status = storage.open(STORAGE_DIR, STORAGE_NAME);

    ASSERT_TRUE(status.isOk());

    {
        auto [status, entry] = storage.load(StorageEngine<>::RootEntryId);

        ASSERT_TRUE(status.isOk());
        ASSERT_EQ(entry.handle(), StorageEngine<>::RootEntryId);

        ASSERT_TRUE(storage.save(entry).isOk());
    }

    ASSERT_TRUE(storage.close().isOk());

    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".logd"));
    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".index"));
    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".logdc"));
}

TEST(StorageTest, Compaction) {
    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".logd"));
    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".index"));
    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".logdc"));

    StorageEngine<> storage;

    StorageEngine<>::OpenOptions opts;
    opts.CompactionRatio = 0.9;
    opts.CompactionDeviceMinSize = 1 * 1024 * 1024; // 1 Mb

    {
        auto status = storage.open(STORAGE_DIR, STORAGE_NAME, opts);

        ASSERT_TRUE(status.isOk());

        {
            auto [status, entry] = storage.load(StorageEngine<>::RootEntryId);

            entry.setProperty("some_property", Property{std::string(4096, 'a')});

            ASSERT_TRUE(status.isOk());
            ASSERT_EQ(entry.handle(), StorageEngine<>::RootEntryId);

            for (std::size_t i = 0; i < 1024; ++i)
                ASSERT_TRUE(storage.save(entry).isOk());
        }

        ASSERT_TRUE(storage.close().isOk());
    }

    {
        auto status = storage.open(STORAGE_DIR, STORAGE_NAME, opts); // should start compaction

        ASSERT_TRUE(status.isOk());

        {
            auto [status, entry] = storage.load(StorageEngine<>::RootEntryId);

            {
                auto [status, value] = entry.property("some_property");

                ASSERT_TRUE(status.isOk());
                ASSERT_EQ(value, Property{std::string(4096, 'a')});
            }

            ASSERT_TRUE(status.isOk());
            ASSERT_EQ(entry.handle(), StorageEngine<>::RootEntryId);
        }

        ASSERT_TRUE(storage.close().isOk());
    }

    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".logd"));
    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".index"));
    SKV_UNUSED(os::File::unlink(STORAGE_DIR + char(os::path::separator) + STORAGE_NAME + ".logdc"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}



