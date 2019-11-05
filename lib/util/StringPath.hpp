#pragma once

#include <string>
#include <string_view>

namespace skv::util {

[[nodiscard]] std::string simplifyPath(std::string_view path);

}
