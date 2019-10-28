#include <sstream>

#include <gtest/gtest.h>

#include <ondisk/Storage.hpp>

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

TEST(StorageTest, OpenClose) {
    Storage<> storage;

    auto status = storage.open(STORAGE_DIR, STORAGE_NAME);

    ASSERT_TRUE(status.isOk());

    {
        auto [status, entry] = storage.load(Storage<>::RootEntryId);

        ASSERT_TRUE(status.isOk());
        ASSERT_EQ(entry.key(), Storage<>::RootEntryId);
    }

    ASSERT_TRUE(storage.close().isOk());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}



