#include <string>
#include <string_view>
#include <vector>

#include "String.hpp"

namespace skv::util {

std::string simplifyPath(const std::string &path) {
    auto pathStack = skv::util::split(path, '/', true);  // split path by '/' symbol skypping empty parts
    std::vector<std::string> result;
    result.reserve(pathStack.size());

    for (auto&& d : pathStack) {
        if (d.empty() || d == ".")
            continue;

        if (d == "..") {
            if (!result.empty())
                result.pop_back();
        }
        else
            result.emplace_back(std::move(d));
    }

    if (result.empty())
        return "/";

    std::string ret;
    ret.reserve(path.size());

    for (auto&& r : result) {
        ret.append("/");
        ret.append(std::begin(r), std::end(r));
    }

    return ret;
}

}
