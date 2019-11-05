#include "StringPathIterator.hpp"
#include "String.hpp"

#include <algorithm>
#include <numeric>

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
    chunks_{util::split(std::move(path), separator)}, valid_{true}
{
}

ReverseStringPathIterator &ReverseStringPathIterator::operator++() {
    if (chunks_.empty() || !valid_) {
        valid_ = false;

        return *this;
    }

	chunks_.pop_back();

    return *this;
}

std::string ReverseStringPathIterator::operator*() {
    return std::accumulate(std::cbegin(chunks_), std::cend(chunks_),
        std::string{ separator },
        [](auto&& path, auto&& chunk) {
            if (path.size() == 1)
                return path + chunk;

            return path + separator + chunk;
        });
}

bool ReverseStringPathIterator::operator!=(const ReverseStringPathIterator &other) {
    return valid_ != other.valid_;
}

ReverseStringPathIterator make_reverse_path_iterator(std::string path) {
    return ReverseStringPathIterator{std::move(path)};
}

}
