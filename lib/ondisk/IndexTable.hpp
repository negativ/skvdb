#pragma once

#include <cstdint>
#include <iostream>
#include <type_traits>

#include <boost/endian/conversion.hpp>

#include "IndexRecord.hpp"
#include "LogDevice.hpp"
#include "btree/btree_map.hpp"

namespace skv::ondisk {

template <typename Key          = std::uint64_t,
          typename BlockIndex   = LogDevice::block_index_type,
          typename BytesCount   = LogDevice::bytes_count_type,
          typename Timestamp    = std::uint64_t>
class IndexTable final {
public:
    using key_type          = Key;
    using block_index_type  = BlockIndex;
    using bytes_count_type  = BytesCount;
    using timestamp_type    = Timestamp;
    using index_record_type = IndexRecord<key_type, block_index_type, bytes_count_type, timestamp_type>;
    using table_type        = btree::btree_map<key_type, index_record_type>;

private:
    table_type table_;
};

template <typename K, typename BI, typename BC, typename TS>
inline std::ostream& operator<<(std::ostream& os, const IndexTable<K, BI, BC, TS>& p)
{
    namespace be = boost::endian;

    static_cast<void>(p);

    return os;
}

template <typename K, typename BI, typename BC, typename TS>
inline std::istream& operator>>(std::istream& is, IndexTable<K, BI, BC, TS>& p)
{
    namespace be = boost::endian;

    static_cast<void>(p);

    return is;
}

}
