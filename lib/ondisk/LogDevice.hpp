#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <tuple>
#include <vector>

#include <util/Status.hpp>

namespace skv::ondisk {

using namespace skv::util;

/**
 * @brief Log-structured block device
 */
class LogDevice final
{
    struct Impl;
public:
    using Buffer        = std::vector<char>;    /* maybe std::uint8_t is better choice */
    using BlockIndex    = std::uint32_t;        /* so we can address up to 16 TB */
    using BlockCount    = std::uint32_t;
    using BytesCount    = Buffer::size_type;

    struct OpenOption {
        std::uint32_t   BlockSize{4096};
        bool            CreateNewIfNotExist{true};
    };

    LogDevice();
    ~LogDevice() noexcept;

    LogDevice(const LogDevice&) = delete;
    LogDevice& operator=(const LogDevice&) = delete;

    LogDevice(LogDevice&&) = delete;
    LogDevice& operator=(LogDevice&&) = delete;

    /**
     * @brief Open block device
     * @param path to file
     * @param options
     * @return
     */
    [[nodiscard]] Status open(std::string_view path, OpenOption options);

    /**
     * @brief Closes block device file
     */
    Status close();

    /**
     * @brief Read "cnt" bytes starting from block index "n"
     * @param n - block index
     * @param cnt - bytes count
     * @return Status of operation and buffer
     */
    [[nodiscard]] std::tuple<Status, Buffer> read(BlockIndex n, BytesCount cnt);

    /**
     * @brief append
     * @param buffer
     * @return
     */
    [[nodiscard]] std::tuple<Status, BlockIndex, BlockCount> append(const Buffer& buffer);

    /**
     * @brief sizeInBytes
     * @return
     */
    std::uint64_t sizeInBytes() const noexcept;

    /**
     * @brief sizeInBlocks
     * @return
     */
    BlockCount sizeInBlocks() const noexcept;

    /**
     * @brief Block size in bytes
     * @return
     */
    std::uint32_t blockSize() const noexcept;

private:
    bool createNew();
    bool initReaders();

    std::unique_ptr<Impl> impl_;
};

}
