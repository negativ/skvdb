#include "StringPathIterator.hpp"

#include <algorithm>

namespace skv::util {

StringPathIterator::StringPathIterator() = default;

StringPathIterator::StringPathIterator(std::string path):
    path_{std::move(path)}, valid_{true}
{
    end = std::find(std::cbegin(path_), std::cend(path_), separator);

    if (std::distance(std::cbegin(path_), end) == 0)  // skipping leading separator
        ++(*this);
}

StringPathIterator &StringPathIterator::operator++() {
    if (!valid_)
        return *this;

    if (end == std::cend(path_)) {
        valid_ = false;

        return *this;
    }

    end = std::find(end + 1, std::cend(path_), separator);

    return *this;
}

std::string StringPathIterator::operator*() {
    return std::string{std::cbegin(path_), end};
}

bool StringPathIterator::operator!=(const StringPathIterator &other) {
    return valid_ != other.valid_;
}

StringPathIterator make_path_iterator(std::string path) {
    return StringPathIterator{std::move(path)};
}

ReverseStringPathIterator::ReverseStringPathIterator() = default;

ReverseStringPathIterator::ReverseStringPathIterator(std::string path):
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

ReverseStringPathIterator &ReverseStringPathIterator::operator++() {
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

std::string ReverseStringPathIterator::operator*() {
    return path_.substr(0, idx + 1);
}

bool ReverseStringPathIterator::operator!=(const ReverseStringPathIterator &other) {
    return valid_ != other.valid_;
}

ReverseStringPathIterator make_reverse_path_iterator(std::string path) {
    return ReverseStringPathIterator{std::move(path)};
}

}
