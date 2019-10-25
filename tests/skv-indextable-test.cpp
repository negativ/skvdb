#include <sstream>

#include <gtest/gtest.h>

#include <ondisk/IndexTable.hpp>

using namespace skv::ondisk;

TEST(IndexTableTest, ReadWrite) {
    [[maybe_unused]] IndexTable<> table;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}


