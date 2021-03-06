#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <unordered_map>

#include "IndexRecord.hpp"
#include "util/Serialization.hpp"
#include "util/Unused.hpp"

namespace skv::ondisk {

template <typename Key,
          typename BlockIndex,
          typename BytesCount>
class IndexTable;

template <typename Key,
          typename BlockIndex,
          typename BytesCount>
std::ostream& operator<<(std::ostream& _os, const IndexTable<Key, BlockIndex, BytesCount>& p);

template <typename Key,
          typename BlockIndex,
          typename BytesCount>
std::istream& operator>>(std::istream& _is, IndexTable<Key, BlockIndex, BytesCount>& p);

/**
 * @brief Index table
 */
template <typename Key          = std::uint64_t,
          typename BlockIndex   = std::uint32_t,
          typename BytesCount   = std::uint32_t> // in one record(!!!)
class IndexTable final {
public:
    using key_type          = std::decay_t<Key>;
    using block_index_type  = std::decay_t<BlockIndex>;
    using bytes_count_type  = std::decay_t<BytesCount>;
    using index_record_type = IndexRecord<key_type, block_index_type, bytes_count_type>;
    using table_type        = std::unordered_map<key_type, index_record_type>;

    using iterator          = typename table_type::iterator;
    using const_iterator    = typename table_type::const_iterator;


    static_assert (std::is_integral_v<key_type> && sizeof(key_type) >= sizeof(std::uint16_t), "Key type should be integral type (16 bit width minimum)");
    static_assert (std::is_integral_v<block_index_type > && sizeof(block_index_type) >= sizeof(std::uint16_t), "BlockIndex type should be integral type (16 bit width minimum)");
    static_assert (std::is_integral_v<bytes_count_type> && sizeof(bytes_count_type) >= sizeof(std::uint16_t), "BytesCount type should be integral type (16 bit width minimum)");

    IndexTable() = default;
    ~IndexTable() noexcept = default;

    IndexTable(const IndexTable&) noexcept(std::is_nothrow_copy_constructible_v<table_type>) = default;
    IndexTable& operator=(const IndexTable&) noexcept(std::is_nothrow_copy_assignable_v<table_type>) = default;

    IndexTable(IndexTable&&) noexcept(std::is_nothrow_move_constructible_v<table_type>) = default;
    IndexTable& operator=(IndexTable&&) noexcept(std::is_nothrow_move_assignable_v<table_type>) = default;

    iterator begin() noexcept(std::is_nothrow_copy_constructible_v<iterator>) {
        return table_.begin();
    }

    const_iterator begin() const noexcept(std::is_nothrow_copy_constructible_v<const_iterator>) {
        return table_.begin();
    }

    iterator end() noexcept(std::is_nothrow_copy_constructible_v<iterator>) {
        return table_.end();
    }

    const_iterator end() const noexcept(std::is_nothrow_copy_constructible_v<const_iterator>) {
        return table_.end();
    }

    [[nodiscard]] iterator find(const key_type& k) noexcept(std::is_nothrow_copy_constructible_v<iterator>) {
        return table_.find(k);
    }

    [[nodiscard]] const_iterator find(const key_type& k) const noexcept(std::is_nothrow_copy_constructible_v<const_iterator>) {
        return table_.find(k);
    }

    [[nodiscard]] bool empty() const {
        return begin() == end();
    }

    bool insert(const index_record_type& idx) {
        table_[idx.key()] = idx;

        return true;
    }

    iterator erase(iterator it) {
        return table_.erase(it);
    }

    iterator erase(const key_type& k) {
        if (auto it = find(k); it != end())
            return table_.erase(it);

        return end();
    }

    std::size_t size() const noexcept {
        return table_.size();
    }

    /**
     * @brief Size of all records on disk (in bytes). Updates only on loading index table from disk. Used for calulation of compaction rate.
     * @return
     */
    std::uint64_t diskFootprint() const noexcept {
        return diskFootprint_;
    }

    /**
     * @brief Size of all records on disk (in blocks). Updates only on loading index table from disk. Used for calulation of compaction rate.
     * @return
     */
    std::uint64_t blockFootprint() const noexcept {
        return blockFootprint_;
    }

    /**
     * @brief Size of disk block
     * @return
     */
    std::uint32_t blockSize() const noexcept {
        return blockSize_;
    }

    /**
     * @brief Sets block size
     * @return
     */
    void setBlockSize(std::uint32_t bs) noexcept {
        blockSize_ = bs;
    }

    [[nodiscard]] bool operator==(const IndexTable& other) const noexcept {
        // disk & block footprint ignored when comparing index tables

        return table_ == other.table_ &&
               blockSize_ == other.blockSize_;
    }

private:
    friend std::istream& operator>> <Key, BlockIndex, BytesCount>(std::istream& is, IndexTable& p);
    friend std::istream& operator>> <Key, BlockIndex, BytesCount>(std::istream& is, IndexTable& p);

    table_type table_;
    std::uint32_t blockSize_{0};
    std::uint64_t diskFootprint_{0};
    std::uint64_t blockFootprint_{0};
};

template <typename Key,
          typename BlockIndex,
          typename BytesCount>
inline std::ostream& operator<<(std::ostream& _os, const IndexTable<Key, BlockIndex, BytesCount>& p)
{
    util::Serializer s{_os};

    std::int64_t d = std::distance(std::cbegin(p), std::cend(p));
    assert(d >= 0);

    s << d;

    std::for_each(std::cbegin(p), std::cend(p),
                  [&s](auto&& p) { s << p.second; });

    return _os;
}

template <typename Key,
          typename BlockIndex,
          typename BytesCount>
inline std::istream& operator>>(std::istream& _is, IndexTable<Key, BlockIndex, BytesCount>& p)
{
    util::Deserializer ds{_is};

    using index_type = typename IndexTable<Key, BlockIndex, BytesCount>::index_record_type;

    std::int64_t d;
    ds >> d;

    for (decltype(d) i = 0; i < d; ++i) {
        index_type idx;

        ds >> idx;

        p.insert(idx);

        p.diskFootprint_ += idx.bytesCount();

        if (p.blockSize() > 0) {
            auto blockCount = idx.bytesCount() / p.blockSize();

            if (idx.bytesCount() % p.blockSize() != 0)
                ++blockCount;

            p.blockFootprint_ += blockCount;
        }
    }

    return _is;
}

}
