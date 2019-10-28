#pragma once

#include <cstdint>
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
    using log_device_type   = LogDevice<block_index_type, block_index_type, std::vector<char>>;
    using entry_type        = Entry<key_type, InvalidEntryId>;

    struct OpenOptions {
        std::uint32_t   LogDeviceBlockSize{4096};
        bool            LogDeviceCreateNewIfNotExist{true};
    };

    Storage() {

    }

    ~Storage() noexcept {
        static_cast<void>(close());
    }

    [[nodiscard]] std::tuple<Status, entry_type> load(const key_type& key) {

    }

    [[nodiscard]] Status save(const entry_type& e) {

    }

    [[nodiscard]] Status remove(const entry_type& e) {

    }

    [[nodiscard]] Status remove(const key_type& key) {

    }

    [[nodiscard]] Status open(std::string_view directory, std::string_view storageName, OpenOptions opts = {}) {
        openOptions_ = opts;

        if (auto status = openDevice(createPath(directory, storageName, LOG_DEVICE_SUFFIX)); !status.isOk())
            return status;
        if (auto status = openIndexTable(createPath(directory, storageName, INDEX_TABLE_SUFFIX)); !status.isOk())
            return status;

        if (indexTable_.empty() && logDevice_.sizeInBlocks() > 0)
            return Status::Fatal("Broken storage");

        directory_ = directory;
        storageName_ = storageName;

        return Status::Ok();
    }

    [[nodiscard]] Status close() {
        auto status1 = closeDevice();
        auto status2 = closeIndexTable();

        if (!status1.isOk())
            return status1;

        return status2;
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
};

}
