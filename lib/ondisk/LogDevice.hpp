#pragma once

#include <atomic>
#include <cassert>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string_view>
#include <thread>
#include <tuple>
#include <vector>

#include "os/File.hpp"
#include "util/Status.hpp"

namespace skv::ondisk {

using namespace skv::util;

/**
 * @brief Log-structured block device
 */
template <typename BlockIndex = std::uint32_t,
          typename BlockCount = std::uint32_t,
          typename Buffer     = std::vector<char>>
class LogDevice final
{
    static constexpr std::size_t MAX_READ_THREADS = 17;
    static constexpr std::uint32_t DEFAULT_BLOCK_SIZE = 4096;
    static constexpr std::uint32_t MIN_BLOCK_SIZE = 512;

public:
    using buffer_type           = std::decay_t<Buffer>;                           /* maybe std::uint8_t is better choice */
    using buffer_value_type     = typename buffer_type::value_type; /* maybe std::uint8_t is better choice */
    using block_index_type      = std::decay_t<BlockIndex>;                       /* so we can address up to 16 TB */
    using block_count_type      = std::decay_t<BlockCount>;
    using bytes_count_type      = typename buffer_type::size_type;

    struct OpenOption {
        std::uint32_t   BlockSize{DEFAULT_BLOCK_SIZE};
        bool            CreateNewIfNotExist{true};
    };

    LogDevice() = default;

    ~LogDevice() noexcept {
        static_cast<void>(close());
    }

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
    [[nodiscard]] Status open(std::string_view path, OpenOption options) {
        if (options.BlockSize % MIN_BLOCK_SIZE != 0)
            return Status::InvalidArgument("Block size should be a multiple of 512 (e.g. 4096)");

        std::unique_lock lock(lock_);

        const auto exists = os::File::exists(path);

        if (!options.CreateNewIfNotExist && !exists)
            return Status::IOError("File not exists.");

        path_ = path;
        openOption_ = options;
        blocks_.store(0);
        opened_ = false;
        writeHandle_.reset();
        fillbuffer.resize(options.BlockSize);

        if (!exists && !createNew())
            return Status::IOError(std::string{"Unable to create block device at: "} + std::string{path});

        auto file = os::File::open(path_, "r+");
        writeHandle_.swap(file);

        if (!writeHandle_)
            return Status::IOError("Unable top open device for writing");

        opened_ = true;
        os::File::seek(writeHandle_, 0, os::File::Seek::End);

        blocks_.store(block_count_type(os::File::tell(writeHandle_) / blockSize()));

        if (!initReaders()) {
            lock_.unlock();

            static_cast<void>(close().isOk());

            return Status::IOError("Unable top open device for reading");
        }

        return Status::Ok();
    }

    /**
     * @brief Closes block device file
     */
    [[nodiscard]] Status close() {
        std::unique_lock lock(lock_);

        if (!opened())
            return Status::Ok();

        opened_ = false;
        writeHandle_.reset();

        for (std::size_t i = 0; i < MAX_READ_THREADS; ++i) {
            std::unique_lock lock(readMutexes_[i]);

            readHandles_[i].reset();
        }

        path_.clear();
        blocks_.store(0);

        buffer_type tmp;
        fillbuffer.swap(tmp);

        return Status::Ok();
    }

    /**
     * @brief Read "cnt" bytes starting from block index "n"
     * @param n - block index
     * @param cnt - bytes count
     * @return Status of operation and buffer
     */
    [[nodiscard]] std::tuple<Status, buffer_type> read(block_index_type n, bytes_count_type cnt) {
        static constexpr std::hash<std::thread::id> hasher;

        auto totalBlocks = blocks_.load();
        auto readBlocks = (cnt / blockSize()) + (cnt % blockSize()? 1 : 0);

        if (!opened())
            return {Status::IOError("Device not opened"), {}};
        if ((n + readBlocks) > totalBlocks)
            return {Status::InvalidArgument(""), {}};

        const auto readerId = hasher(std::this_thread::get_id()) % MAX_READ_THREADS;

        std::unique_lock lock(readMutexes_[readerId]);

        auto& fhandle = readHandles_[readerId];

        if (!fhandle)
            return {Status::IOError("Device not opened"), {}};

        buffer_type data(cnt, '\0');

        os::File::seek(fhandle, n * blockSize(), os::File::Seek::Set);
        os::File::read(data.data(), sizeof(buffer_value_type), cnt, fhandle);

        return {Status::Ok(), data};
    }

    /**
     * @brief append
     * @param buffer
     * @return
     */
    [[nodiscard]] std::tuple<Status, block_index_type, block_count_type> append(const buffer_type& buffer) {
        if (buffer.empty())
            return {Status::InvalidArgument("Unable to write empty buffer"), 0, 0};

        if (!opened())
            return {Status::IOError("Device not opened"), 0, 0};

        std::unique_lock lock(lock_);

        auto& fhandle = writeHandle_;
        const auto cpos = os::File::tell(fhandle);
        const auto bufferSize = buffer.size();

        if (os::File::write(buffer.data(), sizeof(buffer_value_type), bufferSize, fhandle) != bufferSize)
            return {Status::Fatal("Unable to write. Maybe disk is full."), 0, 0};

        if ((bufferSize % blockSize()) != 0) {
            auto n = blockSize() - (bufferSize % blockSize());

            if (os::File::write(fillbuffer.data(), sizeof(buffer_value_type), n, fhandle) != n)
                return {Status::Fatal("Unable to write. Maybe disk is full."), 0, 0};
        }

        os::File::flush(writeHandle_);

        const auto spos = os::File::tell(fhandle);
        const auto blocksAdded = block_count_type(spos - cpos) / blockSize();

        blocks_.fetch_add(blocksAdded);

        return {Status::Ok(), cpos / blockSize(), blocksAdded};
    }

    /**
     * @brief sizeInBytes
     * @return
     */
    [[nodiscard]] std::uint64_t sizeInBytes() const noexcept {
        return uint64_t(openOption_.BlockSize) * sizeInBlocks();
    }

    /**
     * @brief sizeInBlocks
     * @return
     */
    [[nodiscard]] block_count_type sizeInBlocks() const noexcept {
        return blocks_.load();
    }

    /**
     * @brief Block size in bytes
     * @return
     */
    [[nodiscard]] std::uint32_t blockSize() const noexcept {
        return openOption_.BlockSize;
    }

    /**
     * @brief Device is opened
     * @return
     */
    [[nodiscard]] bool opened() const noexcept {
        return opened_;
    }

private:
    bool createNew() {
        return static_cast<bool>(os::File::open(path_, "w"));
    }

    bool initReaders() {
        for (size_t i = 0; i < MAX_READ_THREADS; ++i) {
            auto file = os::File::open(path_, "rb");
            readHandles_[i].swap(file);

            if (!readHandles_[i])
                return false; // calling function will close all opened handles
        }

        return true;
    }

    std::string path_;
    OpenOption openOption_;
    std::atomic<block_count_type> blocks_;
    bool opened_{false};
    os::File::Handle writeHandle_;
    buffer_type fillbuffer;
    std::array<os::File::Handle, MAX_READ_THREADS> readHandles_;
    std::array<std::mutex, MAX_READ_THREADS> readMutexes_;
    std::shared_mutex lock_;
};

}
