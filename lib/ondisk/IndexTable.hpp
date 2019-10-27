#pragma once

#include <cassert>
#include <cstdint>
#include <iostream>
#include <type_traits>

#include <boost/endian/conversion.hpp>

#include "IndexRecord.hpp"
#include "btree/btree_map.hpp"

namespace skv::ondisk {

template <typename Key          = std::uint64_t,
          typename BlockIndex   = std::uint32_t,
          typename BytesCount   = std::uint32_t> // in one record(!!!)
class IndexTable final {
public:
    using key_type          = Key;
    using block_index_type  = BlockIndex;
    using bytes_count_type  = BytesCount;
    using index_record_type = IndexRecord<key_type, block_index_type, bytes_count_type>;
    using table_type        = btree::btree_map<key_type, index_record_type>;

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

    iterator find(const key_type& k) noexcept(std::is_nothrow_copy_constructible_v<iterator>) {
        return table_.find(k);
    }

    const_iterator find(const key_type& k) const noexcept(std::is_nothrow_copy_constructible_v<const_iterator>) {
        return table_.find(k);
    }

    bool empty() const {
        return begin() == end();
    }

    index_record_type& operator[](const key_type& k) {
        return table_[k];
    }

    const index_record_type& operator[](const key_type& k) const {
        return table_[k];
    }

    iterator erase(iterator it) {
        return table_.erase(it);
    }

    iterator erase(const key_type& k) {
        if (find(k) != end())
            return table_.erase(k);

        return end();
    }

    /**
     * @brief Size of all records on disk (in bytes). Updates only on loading index table from disk. Used for calulation of compaction rate.
     * @return
     */
    std::uint64_t diskFootprint() const noexcept {
        return diskFootprint_;
    }

private:
    template <typename K, typename BI, typename BC>
    friend std::istream& operator>>(std::istream& is, IndexTable<K, BI, BC>& p);

    template <typename K, typename BI, typename BC>
    friend std::istream& operator>>(std::istream& is, IndexTable<K, BI, BC>& p);

    table_type table_;
    std::uint64_t diskFootprint_{0};
};

template <typename K, typename BI, typename BC>
inline std::ostream& operator<<(std::ostream& os, const IndexTable<K, BI, BC>& p)
{
    namespace be = boost::endian;

    std::int64_t d = std::distance(std::cbegin(p), std::cend(p));
    assert(d >= 0);

    be::native_to_little_inplace(d);

    os.write(reinterpret_cast<const char*>(&d), sizeof(d));

    std::for_each(std::cbegin(p), std::cend(p),
                  [&os](auto&& p) { os << p.second; });

    return os;
}

template <typename K, typename BI, typename BC>
inline std::istream& operator>>(std::istream& is, IndexTable<K, BI, BC>& p)
{
    namespace be = boost::endian;

    using index_type = typename IndexTable<K, BI, BC>::index_record_type;

    std::int64_t d{0};
    is.read(reinterpret_cast<char*>(&d), sizeof(d));

    be::little_to_native_inplace(d);

    for (decltype(d) i = 0; i < d; ++i) {
        index_type idx;
        is >> idx;

        p[idx.key()] = idx;
        p.diskFootprint_ += idx.bytesCount();
    }

    return is;
}

}
