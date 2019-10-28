#pragma once

#include <cstdint>
#include <iostream>
#include <type_traits>

#include <boost/endian/conversion.hpp>

namespace skv::ondisk {

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

    constexpr key_type key() const noexcept { return key_; }

    constexpr block_index_type  blockIndex() const noexcept { return blockIndex_; }

    constexpr bytes_count_type bytesCount() const noexcept { return bytesCount_; }

    bool operator==(const IndexRecord& other) const noexcept {
        return key_ == other.key_ &&
               blockIndex_ == other.blockIndex_ &&
               bytesCount_ == other.bytesCount_;
    }

    bool operator<(const IndexRecord& other) const noexcept {
        return key_ < other.key_;
    }

private:
    key_type key_{};
    block_index_type  blockIndex_{};
    bytes_count_type bytesCount_{};
};

template <typename K, typename BI, typename BC>
inline std::ostream& operator<<(std::ostream& os, const IndexRecord<K, BI, BC>& p)
{
    namespace be = boost::endian;

    auto k  = be::native_to_little(p.key());
    auto bi = be::native_to_little(p.blockIndex());
    auto bc = be::native_to_little(p.bytesCount());

    os.write(reinterpret_cast<const char*>(&k),  sizeof(k));
    os.write(reinterpret_cast<const char*>(&bi), sizeof(bi));
    os.write(reinterpret_cast<const char*>(&bc), sizeof(bc));

    return os;
}

template <typename K, typename BI, typename BC>
inline std::istream& operator>>(std::istream& is, IndexRecord<K, BI, BC>& p)
{
    namespace be = boost::endian;

    decltype(p.key()) k;
    decltype(p.blockIndex()) bi;
    decltype(p.bytesCount()) bc;

    is.read(reinterpret_cast<char*>(&k),  sizeof(k));
    is.read(reinterpret_cast<char*>(&bi), sizeof(bi));
    is.read(reinterpret_cast<char*>(&bc), sizeof(bc));

    be::little_to_native_inplace(k);
    be::little_to_native_inplace(bi);
    be::little_to_native_inplace(bc);

    p = IndexRecord<K, BI, BC>{k, bi, bc};

    return is;
}

}
