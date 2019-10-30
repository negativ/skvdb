#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace skv::util {

std::vector<std::string> split(std::string_view str, char delim, bool skipEmptyParts = true);
std::string to_string(std::string_view v);

}
