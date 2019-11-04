#pragma once

#include <deque>
#include <string>

namespace skv::util {

/** simple & stupid */
struct StringPathIterator {
    static constexpr char separator = '/';

    using iterator_category = std::forward_iterator_tag;

    StringPathIterator();

    StringPathIterator(std::string path);

    StringPathIterator &operator++();

    std::string operator*();

    bool operator!=(const StringPathIterator& other);

private:
    std::string path_{};
    std::string::const_iterator end;
    bool valid_{false};
};

StringPathIterator make_path_iterator(std::string path);

struct ReverseStringPathIterator {
    static constexpr char separator = '/';

    ReverseStringPathIterator();

    ReverseStringPathIterator(std::string path);

    ReverseStringPathIterator &operator++();

    std::string operator*();

    bool operator!=(const ReverseStringPathIterator& other);

private:
	std::deque<std::string> chunks_;
    bool valid_{ false };
};

ReverseStringPathIterator make_reverse_path_iterator(std::string path);

}
