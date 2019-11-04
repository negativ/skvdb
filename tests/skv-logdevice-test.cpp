#include <algorithm>
#include <atomic>
#include <unordered_map>
#include <thread>

#include <gtest/gtest.h>

#include <ondisk/LogDevice.hpp>
#include <os/File.hpp>

namespace {
#ifdef BUILDING_UNIX
    const char * const BLOCK_DEVICE_TMP_FILE = "/tmp/blockdevice.bin";
#else
    const char * const BLOCK_DEVICE_TMP_FILE = "blockdevice.bin";
#endif
    const size_t N_RECORDS = 512;
    const size_t RECORD_GROW_FACTOR = 128; // GROW_FACTOR bytes for first record and GROW_FACTOR * N_RECORDS for last
}


using namespace skv;
using namespace skv::util;
using namespace skv::ondisk;

class LogDeviceTest: public ::testing::Test {
protected:
    void SetUp() override {
        auto status = device_.open(BLOCK_DEVICE_TMP_FILE, ondisk::LogDevice<>::OpenOption());

        ASSERT_TRUE(status.isOk() && device_.opened());
    }

    void TearDown() override {
        auto status = device_.close();

        ASSERT_TRUE(status.isOk() && !device_.opened());
        ASSERT_TRUE(os::File::unlink(BLOCK_DEVICE_TMP_FILE));
    }

    struct IndexRecord {
        IndexRecord(size_t k, size_t bl, LogDevice<>::block_index_type bi):
            key{k}, bytesLength{bl}, blockIndex{bi}
        {}

        size_t key;
        size_t bytesLength;
        ondisk::LogDevice<>::block_index_type blockIndex;
    };

    void fill() {
        for (size_t i = 0; i < N_RECORDS; ++i) {
            const auto bufferSize = (i + 1) * RECORD_GROW_FACTOR;
            const auto& ret = device_.append(ondisk::LogDevice<>::buffer_type(bufferSize, (i + 1) % 64));

            auto status = std::get<0>(ret);
            auto blockIdx = std::get<1>(ret);
            auto blockCnt = std::get<2>(ret);

            ASSERT_TRUE(status.isOk());

            auto indexPair = indexTable_.try_emplace(i, i, bufferSize, blockIdx);

            ASSERT_TRUE(indexPair.second);
            EXPECT_GT(blockCnt, 0);
        }

        EXPECT_EQ(indexTable_.size(), N_RECORDS);
    }

    ondisk::LogDevice<> device_;
    std::unordered_map<size_t, IndexRecord> indexTable_;
};

TEST_F(LogDeviceTest, ReadWriteTest) {
    fill();

    for (const auto& p : indexTable_) {
        auto key = p.first;
        auto value = p.second;

        const auto bufferSize = value.bytesLength;
        auto blockIdx = value.blockIndex;

        LogDevice<>::buffer_type compareBuffer(bufferSize, (key + 1) % 64);

        const auto& ret = device_.read(blockIdx, bufferSize);

        ASSERT_TRUE(std::get<0>(ret).isOk());
        EXPECT_EQ(std::get<1>(ret), compareBuffer);
    }
}

TEST_F(LogDeviceTest, ReadWriteTestMT) {
    fill();

    std::vector<std::thread> threads;

    std::generate_n(std::back_inserter(threads),
                    std::thread::hardware_concurrency(),
                    [this]{
                        return std::thread([this] {
                            for (const auto& p : indexTable_) {
                                auto key = p.first;
                                auto value = p.second;

                                const auto bufferSize = value.bytesLength;
                                auto blockIdx = value.blockIndex;

                                LogDevice<>::buffer_type compareBuffer(bufferSize, (key + 1) % 64);

                                const auto& ret = device_.read(blockIdx, bufferSize);

                                ASSERT_TRUE(std::get<0>(ret).isOk());
                                EXPECT_EQ(std::get<1>(ret), compareBuffer);
                            }
                        });
                    });

    std::for_each(std::begin(threads),
                  std::end(threads),
                  [](auto&& t) { t.join(); });
}

TEST_F(LogDeviceTest, ReadWriteTestMTOversubscribtion) {
    fill();

    std::vector<std::thread> threads;

    std::generate_n(std::back_inserter(threads),
                    std::thread::hardware_concurrency() * 4,
                    [this]{
                        return std::thread([this] {
                            for (const auto& p : indexTable_) {
                                auto key = p.first;
                                auto value = p.second;

                                const auto bufferSize = value.bytesLength;
                                auto blockIdx = value.blockIndex;

                                LogDevice<>::buffer_type compareBuffer(bufferSize, (key + 1) % 64);

                                const auto& ret = device_.read(blockIdx, bufferSize);

                                ASSERT_TRUE(std::get<0>(ret).isOk());
                                EXPECT_EQ(std::get<1>(ret), compareBuffer);
                            }
                        });
                    });

    std::for_each(std::begin(threads),
                  std::end(threads),
                  [](auto&& t) { t.join(); });
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
