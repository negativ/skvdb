#include <sstream>

#include <gtest/gtest.h>

#include <ondisk/Entry.hpp>

using namespace skv::ondisk;

using E = Entry<std::uint64_t>;

TEST(EntryTest, BasicTest) {
    E root1{0, ""};

    root1.setProperty("test_str_prop", Property{"some text"});
    root1.setProperty("test_int_prop", Property{123});
    root1.setProperty("test_double_prop", Property{8090.0});

    E root2{0, ""};

    root2.setProperty("test_str_prop", Property{"some text"});
    root2.setProperty("test_int_prop", Property{123});
    root2.setProperty("test_double_prop", Property{8090.0});

    ASSERT_EQ(root1, root2);

    E root3 = root1;
    E root4 = std::move(root2);

    ASSERT_EQ(root1, root3);
    ASSERT_EQ(root3, root4);

    ASSERT_EQ(root1.name(), "");
    ASSERT_EQ(root1.key(),  0);

    E root5{1, "dev"};
    root3 = root5;

    ASSERT_EQ(root3, root5);
    ASSERT_NE(root1, root5);

    ASSERT_EQ(root5.name(), "dev");
    ASSERT_EQ(root5.key(),  1);
}

TEST(EntryTest, ChildrenTest) {
    E root{1, ""};
    E dev{root.key() + 1, "dev"};
    E proc{dev.key() + 1, "proc"};

    ASSERT_TRUE(root.addChild(dev).isOk());
    ASSERT_TRUE(root.addChild(proc).isOk());

    auto rootChildren = root.children();

    ASSERT_EQ(rootChildren.size(), 2);
    ASSERT_EQ(dev.parent(), root.key());
    ASSERT_EQ(proc.parent(), root.key());

    ASSERT_TRUE(root.removeChild(dev).isOk());

    rootChildren = root.children();
    ASSERT_EQ(rootChildren.size(), 1);
    ASSERT_EQ(dev.parent(), E::InvalidKey);

    ASSERT_TRUE(root.removeChild(proc).isOk());
    ASSERT_EQ(proc.parent(), E::InvalidKey);

    rootChildren = root.children();
    ASSERT_TRUE(rootChildren.empty());
}

TEST(EntryTest, ReadWriteTest) {
    E root{1, ""};
    E dev{root.key() + 1, "dev"};
    E proc{dev.key() + 1, "proc"};

    ASSERT_TRUE(root.addChild(dev).isOk());
    ASSERT_TRUE(root.addChild(proc).isOk());

    root.setProperty("test_str_prop", Property{"some text"});
    root.setProperty("test_int_prop", Property{123});
    root.setProperty("test_double_prop", Property{8090.0});

    std::stringstream stream;

    stream << root;

    auto buffer = stream.str();

    ASSERT_FALSE(buffer.empty());

    E anotherRoot;
    stream >> anotherRoot;

    ASSERT_EQ(root, anotherRoot);
}


TEST(EntryTest, PropertyTest) {
    E root{0, ""};

    root.setProperty("test_str_prop", Property{"some text"});
    root.setProperty("test_int_prop", Property{123});
    root.setProperty("test_double_prop", Property{8090.0});

    auto props = root.propertiesSet();

    ASSERT_EQ(props.size(), 3);
    ASSERT_TRUE(props.find("test_str_prop") != std::end(props));
    ASSERT_TRUE(props.find("test_int_prop") != std::end(props));
    ASSERT_TRUE(props.find("test_double_prop") != std::end(props));

    ASSERT_TRUE(root.hasProperty("test_str_prop"));
    ASSERT_TRUE(root.hasProperty("test_int_prop"));
    ASSERT_TRUE(root.hasProperty("test_double_prop"));
    ASSERT_FALSE(root.hasProperty("not_exist"));

    {
        const auto& [status, value] = root.property("test_str_prop");
        ASSERT_TRUE(status.isOk());
        ASSERT_EQ(value, Property{"some text"});
    }

    {
        const auto& [status, value] = root.property("test_int_prop");
        ASSERT_TRUE(status.isOk());
        ASSERT_EQ(value, Property{123});
    }

    {
        const auto& [status, value] = root.property("test_double_prop");
        ASSERT_TRUE(status.isOk());
        ASSERT_EQ(value, Property{8090.0});
    }

    {
        const auto& [status, value] = root.property("not_exist");
        ASSERT_FALSE(status.isOk());
    }

    ASSERT_TRUE(root.removeProperty("test_str_prop"));
    ASSERT_TRUE(root.removeProperty("test_int_prop"));
    ASSERT_TRUE(root.removeProperty("test_double_prop"));

    props = root.propertiesSet();

    ASSERT_TRUE(props.empty());

    ASSERT_FALSE(root.hasProperty("test_str_prop"));
    ASSERT_FALSE(root.hasProperty("test_int_prop"));
    ASSERT_FALSE(root.hasProperty("test_double_prop"));
    ASSERT_FALSE(root.hasProperty("not_exist"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}



