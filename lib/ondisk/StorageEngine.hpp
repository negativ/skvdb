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
#include "util/Serialization.hpp"
#include "util/SpinLock.hpp"
#include "util/Status.hpp"

namespace {

const skv::util::Status DeviceNotOpenedStatus   = skv::util::Status::IOError("Device not opened");

}

namespace skv::ondisk {

template <typename KeyT          = std::uint64_t,
          typename BlockIndexT   = std::uint32_t,
          typename BytesCountT   = std::uint32_t,
          typename PropContainerT= std::map<std::string, Property>,
          typename ClockT        = std::chrono::system_clock,
          KeyT _InvalidKey       = 0,
          KeyT _RootKey          = 1>
class StorageEngine final {
public:
    using key_type          = std::decay_t<KeyT>;

    static constexpr key_type InvalidEntryId = _InvalidKey;
    static constexpr key_type RootEntryId = _RootKey;

    using block_index_type  = std::decay_t<BlockIndexT>;
    using bytes_count_type  = std::decay_t<BytesCountT>;
    using index_table_type  = IndexTable<key_type, block_index_type, bytes_count_type>;
    using index_record_type = typename index_table_type::index_record_type;
    using log_device_type   = LogDevice<block_index_type, block_index_type, std::string>;
    using entry_type        = Entry<key_type, PropContainerT, ClockT, InvalidEntryId>;

    static_assert (std::is_integral_v<key_type>, "Key type should be integral");
    static_assert (std::is_unsigned_v<block_index_type>, "Block index type should be unsigned");
    static_assert (std::is_unsigned_v<bytes_count_type>, "Bytes count type should be unsigned");
    static_assert (sizeof (block_index_type) >= sizeof(std::uint32_t), "Block index type should be at least 32 bits long");
    static_assert (sizeof (bytes_count_type) >= sizeof(std::uint32_t), "Bytes count type should be at least 32 bits long");

    struct OpenOptions {
        std::uint32_t   LogDeviceBlockSize{2048};
        bool            LogDeviceCreateNewIfNotExist{true};
    };

    StorageEngine() {

    }

    ~StorageEngine() noexcept {
        static_cast<void>(close());
    }

    StorageEngine(const StorageEngine&) = delete;
    StorageEngine& operator=(const StorageEngine&) = delete;

    StorageEngine(StorageEngine&&) = delete;
    StorageEngine& operator=(StorageEngine&&) = delete;

    [[nodiscard]] std::tuple<Status, entry_type> load(const entry_type& e) {
        return load(e.key());
    }

    [[nodiscard]] std::tuple<Status, entry_type> load(const key_type& key) {
        if (key == InvalidEntryId)
            return {Status::InvalidArgument("Invalid entry id"), {}};

        std::shared_lock locker(xLock_);

        if (!opened())
            return {DeviceNotOpenedStatus, {}};

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
            return DeviceNotOpenedStatus;

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
            return DeviceNotOpenedStatus;

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
            return DeviceNotOpenedStatus;

        auto it = indexTable_.find(key);

        if (it == std::end(indexTable_))
            return Status::InvalidArgument("Key doesnt exist");

        static_cast<void>(indexTable_.erase(it));

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

        opened_ = true;

        directory_ = directory;
        storageName_ = storageName;

        if (indexTable_.find(RootEntryId) == std::cend(indexTable_)) {// creating root index if needed
            locker.unlock();

            auto status = createRootIndex();

            opened_ = status.isOk();

            return status;
        }

        return Status::Ok();
    }

    [[nodiscard]] Status close() {
        std::unique_lock locker(xLock_);

        if (!opened())
            return Status::Ok();

        auto status1 = closeDevice();
        auto status2 = closeIndexTable();

        opened_ = false;

        if (!status1.isOk())
            return status1;

        return status2;
    }

    [[nodiscard]] bool opened() const noexcept {
        return opened_;
    }

    [[nodiscard]] key_type newKey() noexcept {
        std::lock_guard locker(spLock_);

        return (keyCounter_++);
    }

    void reuseKey(key_type key) {
        // TODO: implement key reusage

        static_cast<void>(key);
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
        return logDevice_.close();
    }

    Status openIndexTable(std::string_view path) {
        std::fstream stream{path.data(), std::ios_base::in};

        if (stream.is_open()) {
            Deserializer d{stream};

            d >> keyCounter_
              >> indexTable_;

            stream.flush();
            stream.close();
        }

        return Status::Ok();
    }

    Status closeIndexTable() {
        std::fstream stream{createPath(directory_, storageName_, INDEX_TABLE_SUFFIX), std::ios_base::out};

        if (stream.is_open()) {
            Serializer s{stream};

            s << keyCounter_
              << indexTable_;

            stream.flush();
            stream.close();

            return Status::Ok();
        }

        return Status::IOError("Unable to save index table");
    }

    [[nodiscard]] Status createRootIndex() {
        resetKeyCounter();

        entry_type root{newKey(), ""};

        return save(root);
    }

    void resetKeyCounter() noexcept {
        std::lock_guard locker(spLock_);

        keyCounter_ = RootEntryId;
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
    SpinLock spLock_;
    key_type keyCounter_{0};
    bool opened_{false};
};

}
