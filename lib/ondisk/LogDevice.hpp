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
    using buffer_type           = std::vector<char>;    /* maybe std::uint8_t is better choice */
    using block_index_type      = std::uint32_t;        /* so we can address up to 16 TB */
    using block_count_type      = std::uint32_t;
    using bytes_count_type      = buffer_type::size_type;

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
    [[nodiscard]] std::tuple<Status, buffer_type> read(block_index_type n, bytes_count_type cnt);

    /**
     * @brief append
     * @param buffer
     * @return
     */
    [[nodiscard]] std::tuple<Status, block_index_type, block_count_type> append(const buffer_type& buffer);

    /**
     * @brief sizeInBytes
     * @return
     */
    std::uint64_t sizeInBytes() const noexcept;

    /**
     * @brief sizeInBlocks
     * @return
     */
    block_count_type sizeInBlocks() const noexcept;

    /**
     * @brief Block size in bytes
     * @return
     */
    std::uint32_t blockSize() const noexcept;

    /**
     * @brief Device is opened
     * @return
     */
    bool opened() const noexcept;

private:
    bool createNew();
    bool initReaders();

    std::unique_ptr<Impl> impl_;
};

}
