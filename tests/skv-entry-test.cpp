#include <chrono>
#include <sstream>
#include <thread>

#include <boost/iostreams/stream.hpp>

#include <gtest/gtest.h>

#include <ondisk/ContainerStreamDevice.hpp>
#include <ondisk/Entry.hpp>
#include <util/Unused.hpp>

using namespace skv::ondisk;

using E = Entry;

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
    ASSERT_EQ(root1.handle(),  0);

    E root5{1, "dev"};
    root3 = root5;

    ASSERT_EQ(root3, root5);
    ASSERT_NE(root1, root5);

    ASSERT_EQ(root5.name(), "dev");
    ASSERT_EQ(root5.handle(),  1);

    ASSERT_LT(root1, root5);
    ASSERT_GT(root5, root1);
}

TEST(EntryTest, ChildrenTest) {
    E root{1, ""};
    E dev{root.handle() + 1, "dev"};
    E proc{dev.handle() + 1, "proc"};

    ASSERT_TRUE(root.addChild(dev).isOk());
    ASSERT_TRUE(root.addChild(proc).isOk());

    auto rootChildren = root.children();

    ASSERT_EQ(rootChildren.size(), 2);
    ASSERT_EQ(dev.parent(), root.handle());
    ASSERT_EQ(proc.parent(), root.handle());

    ASSERT_TRUE(root.removeChild(dev).isOk());

    rootChildren = root.children();
    ASSERT_EQ(rootChildren.size(), 1);
    ASSERT_EQ(dev.parent(), IVolume::InvalidHandle);

    ASSERT_TRUE(root.removeChild(proc).isOk());
    ASSERT_EQ(proc.parent(), IVolume::InvalidHandle);

    rootChildren = root.children();
    ASSERT_TRUE(rootChildren.empty());
}

TEST(EntryTest, ReadWriteTest) {
    using namespace std::literals;
    using namespace std::chrono;
    namespace io = boost::iostreams;

    E root{1, ""};
    E dev{root.handle() + 1, "dev"};
    E proc{dev.handle() + 1, "proc"};

    ASSERT_TRUE(root.addChild(dev).isOk());
    ASSERT_TRUE(root.addChild(proc).isOk());

    root.setProperty("test_str_prop", Property{"some text"});
    root.setProperty("test_int_prop", Property{123});
    root.setProperty("test_double_prop", Property{8090.0});

    ASSERT_TRUE((system_clock::now() + 100ms) < (system_clock::now() + 500ms));

    ASSERT_TRUE(root.expireProperty("test_str_prop", 100ms).isOk());
    ASSERT_TRUE(root.expireProperty("test_int_prop", 500ms).isOk());

    std::vector<char> buffer;
    io::stream<ContainerStreamDevice<std::vector<char>>> stream(buffer);

    stream << root;
    stream.flush();

    ASSERT_FALSE(buffer.empty());

    std::this_thread::sleep_for(200ms);

    E anotherRoot;
    stream.seekg(0, BOOST_IOS::beg);

    stream >> anotherRoot;

    ASSERT_EQ(root, anotherRoot);

    ASSERT_FALSE(root.hasProperty("test_str_prop"));
    ASSERT_FALSE(anotherRoot.hasProperty("test_str_prop"));

    std::this_thread::sleep_for(500ms);

    ASSERT_FALSE(root.hasProperty("test_int_prop"));
    ASSERT_FALSE(anotherRoot.hasProperty("test_int_prop"));
}

TEST(EntryTest, PropertyExpireTest) {
    using namespace std::literals;
    using namespace std::chrono;

    E root{0, ""};

    root.setProperty("test_str_prop", Property{"some text"});
    root.setProperty("test_int_prop", Property{123});
    root.setProperty("test_double_prop", Property{8090.0});

    ASSERT_TRUE(root.expireProperty("test_str_prop", 100ms).isOk());
    ASSERT_TRUE(root.expireProperty("test_int_prop", 200ms).isOk());
    ASSERT_TRUE(root.expireProperty("test_double_prop", 300ms).isOk());

    ASSERT_TRUE(root.hasProperty("test_str_prop"));
    ASSERT_TRUE(root.hasProperty("test_int_prop"));
    ASSERT_TRUE(root.hasProperty("test_double_prop"));

    std::this_thread::sleep_for(150ms);

    ASSERT_FALSE(root.hasProperty("test_str_prop"));
    ASSERT_TRUE(root.hasProperty("test_int_prop"));
    ASSERT_TRUE(root.hasProperty("test_double_prop"));

    std::this_thread::sleep_for(100ms);

    ASSERT_FALSE(root.hasProperty("test_str_prop"));
    ASSERT_FALSE(root.hasProperty("test_int_prop"));
    ASSERT_TRUE(root.hasProperty("test_double_prop"));

    std::this_thread::sleep_for(100ms);

    ASSERT_FALSE(root.hasProperty("test_str_prop"));
    ASSERT_FALSE(root.hasProperty("test_int_prop"));
    ASSERT_FALSE(root.hasProperty("test_double_prop"));

    root.setProperty("test_str_prop", Property{"some text"});
    root.setProperty("test_int_prop", Property{123});
    root.setProperty("test_double_prop", Property{8090.0});

    std::this_thread::sleep_for(500ms);

    ASSERT_TRUE(root.hasProperty("test_str_prop"));
    ASSERT_TRUE(root.hasProperty("test_int_prop"));
    ASSERT_TRUE(root.hasProperty("test_double_prop"));
}

TEST(EntryTest, PropertyTest) {
    E root{0, ""};

    root.setProperty("test_str_prop", Property{"some text"});
    root.setProperty("test_int_prop", Property{123});
    root.setProperty("test_double_prop", Property{8090.0});

    auto props = root.properties();

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
        SKV_UNUSED(value);
        ASSERT_FALSE(status.isOk());
    }

    ASSERT_TRUE(root.removeProperty("test_str_prop").isOk());
    ASSERT_TRUE(root.removeProperty("test_int_prop").isOk());
    ASSERT_TRUE(root.removeProperty("test_double_prop").isOk());

    props = root.properties();

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
