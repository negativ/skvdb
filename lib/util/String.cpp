#include "String.hpp"

namespace skv::util {

std::deque<std::string> split(std::string_view str, char delim, bool skipEmptyParts) {
    std::deque<std::string> tokens;
    auto l = 0u, r = 0u;

    for (auto c : str) {
        if (c != delim)
            ++r;
        else {
            if (r - l > 0) {
                auto token = str.substr(l, r - l);

                if (!token.empty() || !skipEmptyParts)
                    tokens.emplace_back(std::cbegin(token), std::cend(token));
            }
            ++r;

            l = r;
        }
    }

    if (r - l > 0) {
        auto token = str.substr(l, r - l);
        tokens.emplace_back(std::cbegin(token), std::cend(token));
    }

    return tokens;
}

std::string to_string(std::string_view v) {
    return std::string{std::cbegin(v), std::cend(v)};
}

}
