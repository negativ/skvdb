#pragma once

#include <cstdint>
#include <iostream>
#include <type_traits>

#include "util/Serialization.hpp"

namespace skv::ondisk {

/**
 * @brief Index table record
 */
template <typename Key          = std::uint64_t,
          typename BlockIndex   = std::uint32_t,
          typename BytesCount   = std::uint32_t>
class IndexRecord final {
public:
    using key_type          = std::decay_t<Key>;
    using block_index_type  = std::decay_t<BlockIndex>;
    using bytes_count_type  = std::decay_t<BytesCount>;

    static_assert (std::is_integral_v<key_type> && sizeof(key_type) >= sizeof(std::uint16_t), "Key type should be integral type (16 bit width minimum)");
    static_assert (std::is_integral_v<block_index_type > && sizeof(block_index_type) >= sizeof(std::uint16_t), "BlockIndex type should be integral type (16 bit width minimum)");
    static_assert (std::is_integral_v<bytes_count_type> && sizeof(bytes_count_type) >= sizeof(std::uint16_t), "BytesCount type should be integral type (16 bit width minimum)");

    constexpr IndexRecord() noexcept = default;

    constexpr IndexRecord(key_type k, block_index_type  bi, bytes_count_type bc) noexcept:
        key_{k}, blockIndex_{bi}, bytesCount_{bc}
    {}

    ~IndexRecord() noexcept = default;

    IndexRecord(const IndexRecord&) noexcept = default;
    IndexRecord& operator=(const IndexRecord&) noexcept = default;

    IndexRecord(IndexRecord&&) noexcept = default;
    IndexRecord& operator=(IndexRecord&&) noexcept = default;

    [[nodiscard]] constexpr key_type key() const noexcept { return key_; }

    [[nodiscard]] constexpr block_index_type  blockIndex() const noexcept { return blockIndex_; }

    [[nodiscard]] constexpr bytes_count_type bytesCount() const noexcept { return bytesCount_; }

    [[nodiscard]] bool operator==(const IndexRecord& other) const noexcept {
        return key_ == other.key_ &&
               blockIndex_ == other.blockIndex_ &&
               bytesCount_ == other.bytesCount_;
    }

    [[nodiscard]] bool operator!=(const IndexRecord& other) const noexcept {
        return !(*this == other);
    }

    [[nodiscard]] bool operator<(const IndexRecord& other) const noexcept {
        return key_ < other.key_;
    }

private:
    key_type key_{};
    block_index_type  blockIndex_{};
    bytes_count_type bytesCount_{};
};

template <typename K, typename BI, typename BC>
inline std::ostream& operator<<(std::ostream& _os, const IndexRecord<K, BI, BC>& p)
{
    util::Serializer s{_os};

    s << p.key()
      << p.blockIndex()
      << p.bytesCount();

    return _os;
}

template <typename K, typename BI, typename BC>
inline std::istream& operator>>(std::istream& _is, IndexRecord<K, BI, BC>& p)
{
    util::Deserializer ds{_is};

    decltype(p.key()) k;
    decltype(p.blockIndex()) bi;
    decltype(p.bytesCount()) bc;

    ds >> k
       >> bi
       >> bc;

    p = IndexRecord<K, BI, BC>{k, bi, bc};

    return _is;
}

}
