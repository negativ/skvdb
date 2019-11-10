#include <sstream>

#include <gtest/gtest.h>

#include <ondisk/IndexRecord.hpp>

using namespace skv::ondisk;

TEST(IndexRecordTest, Basic) {
    IndexRecord<> idx1{1, 2, 3};

    ASSERT_EQ(idx1.key(), 1);
    ASSERT_EQ(idx1.blockIndex(), 2);
    ASSERT_EQ(idx1.bytesCount(), 3);
}


TEST(IndexRecordTest, ReadWrite) {
    IndexRecord<> idx1{1, 2, 3};
    std::stringstream stream;

    stream << idx1;

    auto buffer = stream.str();

    IndexRecord<> idx2;

    stream >> idx2;

    EXPECT_EQ(idx1, idx2);
}

TEST(IndexRecordTest, Compare) {
    IndexRecord<> idx1{1, 2, 3};
    IndexRecord<> idx2{0, 1, 2};
    IndexRecord<> idx3{0, 1, 2};

    ASSERT_NE(idx1, idx2);
    ASSERT_LT(idx2, idx1);
    ASSERT_EQ(idx2, idx3);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}

