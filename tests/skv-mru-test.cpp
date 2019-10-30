#include <cstdint>

#include <gtest/gtest.h>

#include <util/MRUCache.hpp>

using namespace skv::util;

TEST(MRUTest, Basic) {
    MRUCache<std::string, std::uint64_t, 2> cache;

    ASSERT_EQ(cache.size(), 0);
    ASSERT_EQ(cache.capacity(), 2);

    cache.insert("1", 1);
    cache.insert("2", 2);

    std::uint64_t value;

    ASSERT_TRUE(cache.lookup("2", value));
    ASSERT_EQ(value, 2);

    ASSERT_TRUE(cache.lookup("1", value));
    ASSERT_EQ(value, 1);

    ASSERT_FALSE(cache.remove("3"));

    ASSERT_EQ(cache.size(), 2);

    cache.insert("3", 3);

    ASSERT_EQ(cache.size(), 2);

    ASSERT_FALSE(cache.lookup("2", value));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}





