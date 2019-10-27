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

std::ostream& operator<<(std::ostream& os, const Property& p);
std::istream& operator>>(std::istream& is, Property& p);

}
