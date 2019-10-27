#include <sstream>

#include <gtest/gtest.h>

#include <ondisk/Property.hpp>

using namespace skv::ondisk;

TEST(PropertyTest, ReadWriteString) {
    Property prop1{"Some text1"};

    std::stringstream stream;

    stream << prop1;

    auto buffer = stream.str();

    Property prop2;

    stream >> prop2;

    EXPECT_EQ(prop1, prop2);
}

TEST(PropertyTest, ReadWriteInt8) {
    Property prop1{std::int8_t{14}};

    std::stringstream stream;

    stream << prop1;

    auto buffer = stream.str();

    Property prop2;

    stream >> prop2;

    EXPECT_EQ(prop1, prop2);
}

TEST(PropertyTest, ReadWriteUInt8) {
    Property prop1{std::uint8_t{23}};

    std::stringstream stream;

    stream << prop1;

    auto buffer = stream.str();

    Property prop2;

    stream >> prop2;

    EXPECT_EQ(prop1, prop2);
}

TEST(PropertyTest, ReadWriteInt16) {
    Property prop1{std::int16_t{-10000}};

    std::stringstream stream;

    stream << prop1;

    auto buffer = stream.str();

    Property prop2;

    stream >> prop2;

    EXPECT_EQ(prop1, prop2);
}

TEST(PropertyTest, ReadWriteUInt16) {
    Property prop1{std::uint16_t{23000}};

    std::stringstream stream;

    stream << prop1;

    auto buffer = stream.str();

    Property prop2;

    stream >> prop2;

    EXPECT_EQ(prop1, prop2);
}

TEST(PropertyTest, ReadWriteInt32) {
    Property prop1{std::int32_t{-14000000}};

    std::stringstream stream;

    stream << prop1;

    auto buffer = stream.str();

    Property prop2;

    stream >> prop2;

    EXPECT_EQ(prop1, prop2);
}

TEST(PropertyTest, ReadWriteUInt32) {
    Property prop1{std::uint32_t{23000000}};

    std::stringstream stream;

    stream << prop1;

    auto buffer = stream.str();

    Property prop2;

    stream >> prop2;

    EXPECT_EQ(prop1, prop2);
}

TEST(PropertyTest, ReadWriteInt64) {
    Property prop1{std::int64_t{-12000000000}};

    std::stringstream stream;

    stream << prop1;

    auto buffer = stream.str();

    Property prop2;

    stream >> prop2;

    EXPECT_EQ(prop1, prop2);
}

TEST(PropertyTest, ReadWriteUInt64) {
    Property prop1{std::uint64_t{23000000000}};

    std::stringstream stream;

    stream << prop1;

    auto buffer = stream.str();

    Property prop2;

    stream >> prop2;

    EXPECT_EQ(prop1, prop2);
}

TEST(PropertyTest, ReadWriteFloat) {
    Property prop1{float{78.123f}};

    std::stringstream stream;

    stream << prop1;

    auto buffer = stream.str();

    Property prop2;

    stream >> prop2;

    EXPECT_EQ(prop1, prop2);
}

TEST(PropertyTest, ReadWriteDouble) {
    Property prop1{double{78123909.9134}};

    std::stringstream stream;

    stream << prop1;

    auto buffer = stream.str();

    Property prop2;

    stream >> prop2;

    EXPECT_EQ(prop1, prop2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}



