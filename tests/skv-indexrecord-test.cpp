#include <sstream>

#include <gtest/gtest.h>

#include <ondisk/IndexRecord.hpp>

using namespace skv::ondisk;

TEST(IndexRecordTest, ReadWrite) {
    IndexRecord<> idx1{1, 2, 3, 4};
    std::stringstream stream;

    stream << idx1;

    auto buffer = stream.str();

    IndexRecord<> idx2;

    stream >> idx2;

    EXPECT_EQ(idx1, idx2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

