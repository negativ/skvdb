﻿#pragma once

#include <cassert>
#include <cstdint>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <fstream>
#include <vector>

#include <boost/iostreams/stream.hpp>

#include "ContainerStreamDevice.hpp"
#include "Entry.hpp"
#include "IndexTable.hpp"
#include "LogDevice.hpp"
#include "os/File.hpp"
#include "util/Log.hpp"
#include "util/Serialization.hpp"
#include "util/SpinLock.hpp"
#include "util/Status.hpp"
#include "util/String.hpp"
#include "util/Unused.hpp"

namespace skv::ondisk {

/**
 * @brief Storage engine
 */
template <typename KeyT          = std::uint64_t,
          typename BlockIndexT   = std::uint32_t,
          typename BytesCountT   = std::uint32_t,
          typename PropContainerT= std::map<std::string, Property>,
          typename ClockT        = std::chrono::system_clock,
          KeyT _InvalidKey       = 0,
          KeyT _RootKey          = 1>
class StorageEngine final {
    const skv::util::Status DeviceNotOpenedStatus = skv::util::Status::IOError("Device not opened");

public:
    using key_type = std::decay_t<KeyT>;

    static constexpr key_type InvalidEntryId = _InvalidKey;
    static constexpr key_type RootEntryId = _RootKey;

    using block_index_type  = std::decay_t<BlockIndexT>;
    using bytes_count_type  = std::decay_t<BytesCountT>;
    using index_table_type  = IndexTable<key_type, block_index_type, bytes_count_type>;
    using index_record_type = typename index_table_type::index_record_type;
    using buffer_type       = std::vector<char>;
    using log_device_type   = LogDevice<block_index_type, block_index_type, buffer_type>;
    using entry_type        = Entry<key_type, PropContainerT, ClockT, InvalidEntryId>;

    static_assert (std::is_integral_v<key_type>,            "Key type should be integral");
    static_assert (std::is_unsigned_v<block_index_type>,    "Block index type should be unsigned");
    static_assert (std::is_unsigned_v<bytes_count_type>,    "Bytes count type should be unsigned");
    static_assert (sizeof (block_index_type) >= sizeof(std::uint32_t), "Block index type should be at least 32 bits long");
    static_assert (sizeof (bytes_count_type) >= sizeof(std::uint32_t), "Bytes count type should be at least 32 bits long");

    struct OpenOptions {
        static constexpr double         DefaultCompactionRatio{0.6}; // 60%
        static constexpr std::uint64_t  DefaultCompactionDeviceMinSize{std::uint64_t{1024 * 1024 * 1024} * 4}; // 4GB
        static constexpr std::uint32_t  DefaultLogDeviceBlockSize{2048};

        double          CompactionRatio{DefaultCompactionRatio};
        std::uint64_t   CompactionDeviceMinSize{DefaultCompactionDeviceMinSize};
        std::uint32_t   LogDeviceBlockSize{DefaultLogDeviceBlockSize};
        bool            LogDeviceCreateNewIfNotExist{true};
    };

    StorageEngine() = default;

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
        namespace io = boost::iostreams;

        if (key == InvalidEntryId)
            return {Status::InvalidArgument("Invalid entry id"), {}};

        std::shared_lock locker(xLock_);

        if (!opened())
            return {DeviceNotOpenedStatus, {}};

        auto [istatus, index] = getIndexRecord(key);

        locker.unlock();

        if (!istatus.isOk())
            return {Status::InvalidArgument("Key doesnt exist"), {}};

        try {
            auto [status, buffer] = logDevice_.read(index.blockIndex(), index.bytesCount());

            if (!status.isOk())
                return {status, {}};

            io::stream<ContainerStreamDevice<buffer_type>> stream(buffer);
            entry_type e;

            stream.seekg(0, BOOST_IOS::beg);
            stream >> e;

            return {Status::Ok(), e};
        }
        catch (const std::exception& e) {
            Log::e("StoreEngine", "Exception when loading entry: ", e.what());
        }

        return {Status::Fatal("Unknown error"), {}};
    }

    [[nodiscard]] Status save(const entry_type& e) {
        namespace io = boost::iostreams;

        if (e.key() == InvalidEntryId)
            return Status::InvalidArgument("Invalid entry id");

        buffer_type buffer;
        io::stream<ContainerStreamDevice<buffer_type>> stream(buffer);

        try {
            stream << e;
            stream.flush();

            if (buffer.empty())
                return Status::Fatal("Unable to serialize entry!");

            if (sizeof(bytes_count_type) < sizeof(std::uint64_t)) { // overflow check
                constexpr std::uint64_t max_bytes_count = std::numeric_limits<bytes_count_type>::max();

                if (buffer.size() > max_bytes_count)
                    return  Status::IOError("Entry to big");
            }
        }
        catch (const std::exception& e) {
            Log::e("StoreEngine", "Exception when saving entry: ", e.what());

            return Status::Fatal(e.what());
        }

        std::unique_lock locker(xLock_);

        if (!opened())
            return DeviceNotOpenedStatus;

        auto [status, blockIndex, blockCount] = logDevice_.append(buffer);

        assert(blockCount >= 1);
        SKV_UNUSED(blockCount);

        if (!status.isOk())
            return status;

        return insertIndexRecord(index_record_type{e.key(), blockIndex, bytes_count_type(buffer.size())});
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

        logDevicePath_ = createPath(directory, storageName, LOG_DEVICE_SUFFIX);
        idxtPath_   = createPath(directory, storageName, INDEX_TABLE_SUFFIX);

        if (auto status = openDevice(logDevicePath_); !status.isOk())
            return status;
        if (auto status = openIndexTable(idxtPath_); !status.isOk())
            return status;

        if (indexTable_.empty() && logDevice_.sizeInBlocks() > 0)
            return Status::Fatal("Broken storage");

        opened_ = true;

        directory_ = directory;
        storageName_ = storageName;

        Status status = Status::Ok();
        auto [istatus, index] = getIndexRecord(RootEntryId);

        SKV_UNUSED(index);

        if (!istatus.isOk()) {// creating root index if needed
            locker.unlock();

            status = createRootIndex();

            opened_ = status.isOk();
        }

        if (status.isOk())
            return doOfflineCompaction();

        return status;
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
    const std::string INDEX_TABLE_SUFFIX       = ".index";
    const std::string LOG_DEVICE_SUFFIX        = ".logd";
    const std::string LOG_DEVICE_COMP_SUFFIX   = ".logdc";

    [[nodiscard]] std::tuple<Status, index_record_type> getIndexRecord(key_type key) const {
        auto it = indexTable_.find(key);

        if (it == std::cend(indexTable_))
            return {Status::InvalidArgument("Key doesnt exist"), {}};

        return {Status::Ok(), it->second};
    }

    [[nodiscard]] Status insertIndexRecord(const index_record_type& index) {
        if (indexTable_.insert(index))
            return Status::Ok();

        return Status::Fatal("Unknown error");
    }

    [[nodiscard]] Status openDevice(std::string_view path) {
        typename log_device_type::OpenOption opts;
        opts.BlockSize = openOptions_.LogDeviceBlockSize;
        opts.CreateNewIfNotExist = openOptions_.LogDeviceCreateNewIfNotExist;

        return logDevice_.open(path, opts);
    }

    [[nodiscard]] Status closeDevice() {
        return logDevice_.close();
    }

    [[nodiscard]] Status openIndexTable(std::string_view path) {
        std::fstream stream{path.data(), std::ios_base::in};

        indexTable_.setBlockSize(openOptions_.LogDeviceBlockSize);

        if (stream.is_open()) {
            Deserializer d{stream};

            d >> keyCounter_
              >> indexTable_;

            stream.flush();
            stream.close();
        }

        return Status::Ok();
    }

    [[nodiscard]] Status closeIndexTable() {
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


    [[nodiscard]] std::string createPath(std::string_view directory, std::string_view storageName, std::string_view suffix) {
        std::stringstream stream;
        stream << directory << os::File::sep() << storageName << suffix;

        return stream.str();
    }

    void resetKeyCounter() noexcept {
        std::lock_guard locker(spLock_);

        keyCounter_ = RootEntryId;
    }

    [[nodiscard]] Status doOfflineCompaction() {
        if (logDevice_.sizeInBytes() < openOptions_.CompactionDeviceMinSize)
            return Status::Ok();

        const auto idxtBlocksOccupied = indexTable_.blockFootprint();
        const auto diskBlocksOccupied = logDevice_.sizeInBlocks();

        double ratio = double(idxtBlocksOccupied) / double(diskBlocksOccupied);

        if (ratio > openOptions_.CompactionRatio)
            return Status::Ok();

        index_table_type idxtCompacted;
        typename log_device_type::OpenOption opts;
        auto path = createPath(directory_, storageName_, LOG_DEVICE_COMP_SUFFIX);

        SKV_UNUSED(os::File::unlink(path));

        opts.BlockSize = openOptions_.LogDeviceBlockSize;
        opts.CreateNewIfNotExist = true;

        log_device_type device;

        if (!device.open(path, opts).isOk())
            return Status::IOError("Unable to open device");

        Status compStatus = Status::Ok();

        for (const auto& p : indexTable_) {
            auto [key, index] = p;
            auto [status, buffer] = logDevice_.read(index.blockIndex(), index.bytesCount());

            if (!status.isOk()) {
                compStatus = status;

                break;
            }

            auto [appendStatus, blockIndex, blockCount] = device.append(buffer);

            assert(blockCount >= 1);
            SKV_UNUSED(blockCount);

            if (!appendStatus.isOk()) {
                compStatus = status;

                break;
            }

            auto inserted = idxtCompacted.insert(index_record_type{key, blockIndex, bytes_count_type(buffer.size())});
            SKV_UNUSED(inserted);
        }

        if (!compStatus.isOk()) {
            SKV_UNUSED(device.close());

            SKV_UNUSED(os::File::unlink(path));

            return Status::IOError("Unable to compact device");
        }

        SKV_UNUSED(logDevice_.close());
        SKV_UNUSED(device.close());

        if (!os::File::unlink(logDevicePath_)) {
            SKV_UNUSED(os::File::unlink(path));

            return openDevice(logDevicePath_);
        }

        if (!os::File::rename(path, logDevicePath_))
            return Status::Fatal("Unable to rename device");

        if (auto status = openDevice(logDevicePath_); status.isOk()) {
            indexTable_ = std::move(idxtCompacted);

            return status;
        }

        return Status::Fatal("Unable to compact device");
    }

    index_table_type indexTable_;
    log_device_type logDevice_;
    OpenOptions openOptions_;
    std::string directory_;
    std::string storageName_;
    std::string logDevicePath_;
    std::string idxtPath_;
    std::shared_mutex xLock_;
    SpinLock<> spLock_;
    key_type keyCounter_{0};
    bool opened_{false};
};

}
