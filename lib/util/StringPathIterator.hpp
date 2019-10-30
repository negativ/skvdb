#pragma once

#include <algorithm>
#include <string>

namespace skv::util {

/** simple & stupid */
struct StringPathIterator {
    static constexpr char separator = '/';

    using iterator_category = std::forward_iterator_tag;

    StringPathIterator() {

    }

    StringPathIterator(std::string path):
        path_{std::move(path)}, valid_{true}
    {
        end = std::find(std::cbegin(path_), std::cend(path_), separator);

        if (std::distance(std::cbegin(path_), end) == 0)  // skipping leading separator
            ++(*this);
    }

    StringPathIterator& operator++() {
        if (!valid_)
            return *this;

        if (end == std::cend(path_)) {
            valid_ = false;

            return *this;
        }

        end = std::find(end + 1, std::cend(path_), separator);

        return *this;
    }

    std::string operator*() {
        return std::string{std::cbegin(path_), end};
    }

    bool operator!=(const StringPathIterator& other) {
        return valid_ != other.valid_;
    }

private:
    std::string path_{};
    std::string::const_iterator end;
    bool valid_{false};
};

static inline StringPathIterator make_path_iterator(std::string path) {
    return StringPathIterator{std::move(path)};
}

struct ReverseStringPathIterator {
    static constexpr char separator = '/';

    ReverseStringPathIterator() {

    }

    ReverseStringPathIterator(std::string path):
        path_{std::move(path)}, valid_{true}
    {
        if (path_.empty())
            valid_ = false;
        else {
            idx = path_.size() - 1;

            if (path[idx] == separator)
                --idx;
        }
    }

    ReverseStringPathIterator& operator++() {
        if (!valid_)
            return *this;

        while (idx != 0 && path_[idx] != separator) {
            --idx;
        }

        if (idx == 0) {
            valid_ = false;

            return *this;
        }

        --idx;

        return *this;
    }

    std::string operator*() {
        return path_.substr(0, idx + 1);
    }

    bool operator!=(const ReverseStringPathIterator& other) {
        return valid_ != other.valid_;
    }

private:
    std::string path_{};
    std::size_t idx{0};
    bool valid_{false};
};

static inline ReverseStringPathIterator make_reverse_path_iterator(std::string path) {
    return ReverseStringPathIterator{std::move(path)};
}

}
