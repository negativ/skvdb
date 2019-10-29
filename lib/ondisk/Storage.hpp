#pragma once

#include <cstdint>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <fstream>
#include <vector>

#include "Entry.hpp"
#include "IndexTable.hpp"
#include "LogDevice.hpp"
#include "os/File.hpp"
#include "util/Status.hpp"

namespace skv::ondisk {

template <typename Key          = std::uint64_t,
          typename BlockIndex   = std::uint32_t,
          typename BytesCount   = std::uint32_t>
class Storage final {
public:
    using key_type          = std::decay_t<Key>;

    static constexpr key_type InvalidEntryId = 0;
    static constexpr key_type RootEntryId = 1;

    using block_index_type  = std::decay_t<BlockIndex>;
    using bytes_count_type  = std::decay_t<BytesCount>;
    using index_table_type  = IndexTable<key_type, block_index_type, bytes_count_type>;
    using index_record_type = typename index_table_type::index_record_type;
    using log_device_type   = LogDevice<block_index_type, block_index_type, std::string>;
    using entry_type        = Entry<key_type, InvalidEntryId>;

    static_assert (std::is_integral_v<key_type>, "Key type should be integral");
    static_assert (std::is_unsigned_v<block_index_type>, "Block index type should be unsigned");
    static_assert (std::is_unsigned_v<bytes_count_type>, "Bytes count type should be unsigned");
    static_assert (sizeof (block_index_type) >= sizeof(std::uint32_t), "Block index type should be at least 32 bits long");
    static_assert (sizeof (bytes_count_type) >= sizeof(std::uint32_t), "Bytes count type should be at least 32 bits long");

    struct OpenOptions {
        std::uint32_t   LogDeviceBlockSize{4096};
        bool            LogDeviceCreateNewIfNotExist{true};
    };

    Storage() {

    }

    ~Storage() noexcept {
        static_cast<void>(close());
    }

    [[nodiscard]] std::tuple<Status, entry_type> load(const entry_type& e) {
        return load(e.key());
    }

    [[nodiscard]] std::tuple<Status, entry_type> load(const key_type& key) {
        if (key == InvalidEntryId)
            return {Status::InvalidArgument("Invalid entry id"), {}};

        std::shared_lock locker(xLock_);

        if (!opened())
            return {Status::IOError("Device not opened!"), {}};

        auto it = indexTable_.find(key);

        if (it == std::cend(indexTable_))
            return {Status::InvalidArgument("Key doesnt exist"), {}};

        index_record_type index = it->second;

        const auto& [status, buffer] = logDevice_.read(index.blockIndex(), index.bytesCount());

        locker.unlock();

        if (!status.isOk())
            return {status, {}};

        // TODO: implement EntryReader
        std::stringstream stream{buffer, std::ios_base::in};
        entry_type e;

        stream >> e;

        return {Status::Ok(), e};
    }

    [[nodiscard]] Status save(const entry_type& e) {
        if (e.key() == InvalidEntryId)
            return Status::InvalidArgument("Invalid entry id");

        if (!opened())
            return Status::IOError("Device not opened!");

        // TODO: implement EntryWriter
        std::stringstream stream{std::ios_base::out};
        stream << e;

        const auto& buffer = stream.str();

        if (buffer.empty())
            return Status::Fatal("Unable to serialize entry!");

        if (sizeof(bytes_count_type) < sizeof(std::uint64_t)) { // overflow check
            std::uint64_t max_bytes_count = std::numeric_limits<bytes_count_type>::max();

            if (buffer.size() > max_bytes_count)
                return  Status::IOError("Entry to big");
        }

        std::unique_lock locker(xLock_);

        if (!opened())
            return Status::IOError("Device not opened!");

        auto [status, blockIndex, blockCount] = logDevice_.append(buffer);

        if (!status.isOk())
            return status;

        index_record_type index(e.key(), blockIndex, bytes_count_type(buffer.size()));
        indexTable_[e.key()] = index;

        return Status::Ok();
    }

    [[nodiscard]] Status remove(const entry_type& e) {
        return remove(e.key());
    }

    [[nodiscard]] Status remove(const key_type& key) {
        std::unique_lock locker(xLock_);

        if (!opened())
            return Status::IOError("Device not opened");

        auto it = indexTable_.find(key);

        if (it == std::end(indexTable_))
            return Status::InvalidArgument("Key doesnt exist");

        indexTable_.erase(it);

        return Status::Ok();
    }

    [[nodiscard]] Status open(std::string_view directory, std::string_view storageName, OpenOptions opts = {}) {
        std::unique_lock locker(xLock_);

        openOptions_ = opts;

        if (auto status = openDevice(createPath(directory, storageName, LOG_DEVICE_SUFFIX)); !status.isOk())
            return status;
        if (auto status = openIndexTable(createPath(directory, storageName, INDEX_TABLE_SUFFIX)); !status.isOk())
            return status;

        if (indexTable_.empty() && logDevice_.sizeInBlocks() > 0)
            return Status::Fatal("Broken storage");

        directory_ = directory;
        storageName_ = storageName;

        if (indexTable_.find(RootEntryId) == std::cend(indexTable_)) {// creating root index if needed
            locker.unlock();

            auto status = createRootIndex();

            opened_ = status.isOk();

            return status;
        }

        opened_ = true;

        return Status::Ok();
    }

    [[nodiscard]] Status close() {
        std::unique_lock locker(xLock_);

        auto status1 = closeDevice();
        auto status2 = closeIndexTable();

        opened_ = false;

        if (!status1.isOk())
            return status1;

        return status2;
    }

    bool opened() const noexcept {
        return opened_;
    }

private:
    static constexpr const char * const INDEX_TABLE_SUFFIX  = ".index";
    static constexpr const char * const LOG_DEVICE_SUFFIX   = ".logd";

    Status openDevice(std::string_view path) {
        typename log_device_type::OpenOption opts;
        opts.BlockSize = openOptions_.LogDeviceBlockSize;
        opts.CreateNewIfNotExist = openOptions_.LogDeviceCreateNewIfNotExist;

        return logDevice_.open(path, opts);
    }

    Status closeDevice() {
        logDevice_.close();

        return Status::Ok();
    }

    Status openIndexTable(std::string_view path) {
        std::fstream stream{path.data(), std::ios_base::in};

        if (stream.is_open()) {
            stream >> indexTable_;

            stream.close();
        }

        return Status::Ok();
    }

    Status closeIndexTable() {
        std::fstream stream{createPath(directory_, storageName_, INDEX_TABLE_SUFFIX), std::ios_base::out};

        if (stream.is_open()) {
            stream << indexTable_;
            stream.close();

            return Status::Ok();
        }

        return Status::IOError("Unable to save index table");
    }

    [[nodiscard]] Status createRootIndex() {
        entry_type root{RootEntryId, ""};

        return save(root);
    }

    std::string createPath(std::string_view directory, std::string_view storageName, std::string_view suffix) {
        std::stringstream stream;
        stream << directory << os::File::sep() << storageName << suffix;

        return stream.str();
    }

    index_table_type indexTable_;
    log_device_type logDevice_;
    OpenOptions openOptions_;
    std::string directory_;
    std::string storageName_;
    std::shared_mutex xLock_;
    bool opened_{false};
};

}
