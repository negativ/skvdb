#include <sstream>

#include <gtest/gtest.h>

#include <ondisk/IndexTable.hpp>

using namespace skv::ondisk;

TEST(IndexTableTest, Basic) {
    IndexTable<> table;

    ASSERT_TRUE(table.insert(IndexTable<>::index_record_type(0,  1,  2)));
    ASSERT_TRUE(table.insert(IndexTable<>::index_record_type(3,  4,  5)));
    ASSERT_TRUE(table.insert(IndexTable<>::index_record_type(6,  7,  8)));
    ASSERT_TRUE(table.insert(IndexTable<>::index_record_type(9,  10, 11)));
    ASSERT_TRUE(table.insert(IndexTable<>::index_record_type(12, 13, 14)));

    ASSERT_EQ(std::distance(std::cbegin(table), std::cend(table)), 5);
    ASSERT_EQ(table.size(), 5);
    ASSERT_FALSE(table.empty());

    ASSERT_TRUE(IndexTable<>{}.empty());

    ASSERT_NE(table.erase(0), std::end(table));
    ASSERT_EQ(std::distance(std::cbegin(table), std::cend(table)), 4);
    ASSERT_EQ(table.size(), 4);
}

TEST(IndexTableTest, ReadWrite) {
    IndexTable<> table;
    std::stringstream stream;

    ASSERT_TRUE(table.insert(IndexTable<>::index_record_type(0,  1,  2)));
    ASSERT_TRUE(table.insert(IndexTable<>::index_record_type(3,  4,  5)));
    ASSERT_TRUE(table.insert(IndexTable<>::index_record_type(6,  7,  8)));
    ASSERT_TRUE(table.insert(IndexTable<>::index_record_type(9,  10, 11)));
    ASSERT_TRUE(table.insert(IndexTable<>::index_record_type(12, 13, 14)));

    stream << table;

    IndexTable<> rtable;
    stream >> rtable;

    ASSERT_EQ(table, rtable);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}


