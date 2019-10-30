#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <variant>

namespace skv::ondisk {

using Property = std::variant<std::uint8_t, std::int8_t,
                              std::uint16_t, std::int16_t,
                              std::uint32_t, std::int32_t,
                              std::uint64_t, std::int64_t,
                              float, double,
                              std::string>;

inline static constexpr std::uint16_t PropertyIndexMask = 0xFFFF;

static_assert (std::variant_size_v<Property> <= PropertyIndexMask, "Property type list too big");

}

namespace std {
    std::ostream& operator<<(std::ostream& os, const skv::ondisk::Property& p);
    std::istream& operator>>(std::istream& is, skv::ondisk::Property& p);
}
