#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <thread>
#include <tuple>
#include <vector>

#include "os/File.hpp"
#include "util/Status.hpp"
#include "util/Unused.hpp"

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
    static constexpr std::size_t   MAX_READ_THREADS = 17;
    static constexpr std::uint32_t DEFAULT_BLOCK_SIZE = 2048;
    static constexpr std::uint32_t MIN_BLOCK_SIZE = 2048;

public:
    using buffer_type           = std::decay_t<Buffer>;                           /* maybe std::uint8_t is better choice */
    using buffer_value_type     = typename buffer_type::value_type; /* maybe std::uint8_t is better choice */
    using block_index_type      = std::decay_t<BlockIndex>;                       /* so we can address up to 16 TB */
    using block_count_type      = std::decay_t<BlockCount>;
    using bytes_count_type      = typename buffer_type::size_type;

    static_assert (std::is_unsigned_v<block_index_type>, "block_index_type should be unsigned");
    static_assert (std::is_unsigned_v<block_count_type>, "block_count_type should be unsigned");
    static_assert (std::is_unsigned_v<bytes_count_type>, "bytes_count_type should be unsigned");

    struct OpenOption {
        OpenOption() = default;
        std::uint32_t   BlockSize{DEFAULT_BLOCK_SIZE};
        bool            CreateNewIfNotExist{true};
    };

    LogDevice() = default;

    ~LogDevice() noexcept {
        close();
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
    [[nodiscard]] Status open(const os::path& path, OpenOption options) {
        if (options.BlockSize % MIN_BLOCK_SIZE != 0)
            return Status::InvalidArgument("Invalid block size");

        std::unique_lock lock(lock_);

        const auto exists = os::fs::exists(path);

        if (!options.CreateNewIfNotExist && !exists)
            return Status::IOError("File not exists.");

        path_ = path;
        openOption_ = options;
        blocks_.store(0);
        opened_ = false;
        writeHandle_.reset();
        fillbuffer.resize(options.BlockSize);

        if (!exists && !createNew())
            return Status::IOError("Unable to create block device");

        auto file = os::File::open(path_, "rb+");
        writeHandle_.swap(file);

        if (!writeHandle_)
            return Status::IOError("Unable to open device");

        opened_ = true;

        SKV_UNUSED(os::File::seek(writeHandle_, 0, os::File::Seek::End));

        blocks_.store(block_count_type(os::File::tell(writeHandle_) / blockSize()));

        if (!initReaders()) {
            lock_.unlock();

            close();

            return Status::IOError("Unable to open device");
        }

        return Status::Ok();
    }

    /**
     * @brief Close block device file
     */
    Status close() {
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
     * @return {Status::Ok(), data} on success
     */
    [[nodiscard]] std::tuple<Status, buffer_type> read(block_index_type n, bytes_count_type cnt) {
        if (cnt == 0)
            return {Status::InvalidArgument("Invalid count"), {}};

        buffer_type buffer;
        auto status = read(n, buffer, cnt);

        return {status, status.isOk()? buffer : buffer_type{}};
    }

    /**
     * @brief Read "cnt" bytes starting from block index "n"
     * @param n - block index
     * @param buffer - buffer for data. if buffer.size() < cnt buffer will be reallocated
     * @param cnt - bytes count
     * @return {Status::Ok(), data} on success
     */
    [[nodiscard]] Status read(block_index_type n, buffer_type& buffer, bytes_count_type cnt) {
        static constexpr std::hash<std::thread::id> hasher;

        if (cnt == 0)
            return Status::InvalidArgument("Empty buffer");

        if (buffer.size() < cnt) {
            try {
                buffer.resize(cnt);
            }
            catch (...) {
                return Status::Fatal("Out of memory");
            }
        }

        auto totalBlocks = sizeInBlocks();
        auto readBlocks = (cnt / blockSize()) + (cnt % blockSize()? 1 : 0);

        if (!opened())
            return Status::IOError("Device not opened");
        if ((n + readBlocks) > totalBlocks)
            return Status::InvalidArgument("Out of memory");

        const auto readerId = hasher(std::this_thread::get_id()) % MAX_READ_THREADS;

        std::unique_lock lock(readMutexes_[readerId]);

        auto& fhandle = readHandles_[readerId];

        if (!fhandle)
            return Status::IOError("Device not opened");

        if (!os::File::seek(fhandle, std::int64_t(n) * std::int64_t(blockSize()), os::File::Seek::Set))
            return Status::IOError("Unable to seek");

        if (!os::File::read(buffer.data(), sizeof(buffer_value_type), cnt, fhandle))
            return Status::IOError("Unable to seek");

        return Status::Ok();
    }

    /**
     * @brief Append data to device
     * @param buffer
     * @return {Status::Ok(), index of written block, total written block count} on success
     */
    [[nodiscard]] std::tuple<Status, block_index_type, block_count_type> append(const buffer_type& buffer, bytes_count_type bufferSize = 0) {
        if (buffer.empty())
            return {Status::InvalidArgument("Unable to write empty buffer"), 0, 0};

        if (!opened())
            return {Status::IOError("Device not opened"), 0, 0};

        std::unique_lock lock(lock_);

        auto& fhandle = writeHandle_;
        const auto cpos = os::File::tell(fhandle);

        if (bufferSize == 0)
            bufferSize = buffer.size();
        else
            bufferSize = std::min(bufferSize, buffer.size());

        if (os::File::write(buffer.data(), sizeof(buffer_value_type), bufferSize, fhandle) != bufferSize)
            return {Status::Fatal("Unable to write."), 0, 0};

        if ((bufferSize % blockSize()) != 0) {
            auto n = blockSize() - (bufferSize % blockSize());

            if (os::File::write(fillbuffer.data(), sizeof(buffer_value_type), n, fhandle) != n)
                return {Status::Fatal("Unable to write."), 0, 0};
        }

        os::File::flush(writeHandle_);

        const auto spos = os::File::tell(fhandle);
        const auto blocksAdded = block_count_type(spos - cpos) / blockSize();

        blocks_.fetch_add(blocksAdded);

        return {Status::Ok(), block_index_type(cpos / blockSize()), block_count_type(blocksAdded)};
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

    os::path path_;
    OpenOption openOption_;
    std::atomic<block_count_type> blocks_{0};
    bool opened_{false};
    os::File::Handle writeHandle_;
    buffer_type fillbuffer;
    std::array<os::File::Handle, MAX_READ_THREADS> readHandles_;
    std::array<std::mutex, MAX_READ_THREADS> readMutexes_;
    std::shared_mutex lock_;
};

}
