#pragma once

#include <deque>
#include <string>
#include <string_view>

namespace skv::util {

[[nodiscard]] std::deque<std::string> split(std::string_view str, char delim, bool skipEmptyParts = true);
[[nodiscard]] std::string to_string(std::string_view v);

}
