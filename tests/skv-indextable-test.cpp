#include <sstream>

#include <gtest/gtest.h>

#include <ondisk/IndexTable.hpp>

using namespace skv::ondisk;

TEST(IndexTableTest, ReadWrite) {
    IndexTable<> table;
    std::stringstream stream;

    table[0] = IndexTable<>::index_record_type(0,  1,  2);
    table[1] = IndexTable<>::index_record_type(3,  4,  5);
    table[2] = IndexTable<>::index_record_type(6,  7,  8);
    table[3] = IndexTable<>::index_record_type(9,  10, 11);
    table[4] = IndexTable<>::index_record_type(12, 13, 14);

    stream << table;

    std::cout << stream.str() << std::endl;

    IndexTable<> rtable;
    stream >> rtable;

    std::cout << rtable[0].key() << rtable[4].key() << std::endl;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}


