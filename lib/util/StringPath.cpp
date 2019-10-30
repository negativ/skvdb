#pragma once

#include <deque>
#include <numeric>
#include <string>
#include <string_view>

#include "String.hpp"

namespace skv::util {

static inline std::string simplifyPath(std::string_view path) {
    auto pathStack = skv::util::split(path, '/', true);  // split path by '/' symbol skypping empty parts
    std::deque<std::string> result;

    for (const auto& d : pathStack) {
        if (d.empty() || d == ".")
            continue;
        else if (d == "..") {
            if (!result.empty())
                result.pop_back();
        }
        else
            result.push_back(d);
    }

    if (result.empty())
        return "/";

    return std::accumulate(result.begin(), result.end(),
                           std::string{""},
                           [](auto&& path, auto&& d) {return std::move(path + "/" + d); });
}

}
