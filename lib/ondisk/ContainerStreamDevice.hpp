#pragma once

#include <algorithm>
#include <iostream>
#include <vector>

#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/positioning.hpp>

namespace skv::ondisk {

namespace io = boost::iostreams;

template<typename Container = std::vector<char>>
class ContainerStreamDevice {
public:
    using container_type    = std::decay_t<Container>;
    using char_type         = typename Container::value_type;
    using category          = io::seekable_device_tag;

    ContainerStreamDevice(Container& container) noexcept
        : container_(container), pos_(0)
    { }

    ~ContainerStreamDevice() noexcept = default;

    std::streamsize read(char_type* s, std::streamsize n) {
        std::streamsize amt = static_cast<std::streamsize>(container_.size() - pos_);
        std::streamsize result = std::min(n, amt);

        if (result != 0) {
            std::copy(container_.begin() + pos_,
                      container_.begin() + pos_ + result,
                      s);

            pos_ += result;

            return result;
        }

        return -1;
    }
    std::streamsize write(const char_type* s, std::streamsize n) {
        std::streamsize result = 0;

        if (pos_ != container_.size()) {
            std::streamsize amt = static_cast<std::streamsize>(container_.size() - pos_);
            result = std::min(n, amt);

            std::copy(s, s + result, container_.begin() + pos_);

            pos_ += result;
        }

        if (result < n) {
            container_.insert(container_.end(), s + result, s + n);
            pos_ = container_.size();
        }

        return n;
    }

    io::stream_offset seek(io::stream_offset off, std::ios_base::seekdir way) {
        io::stream_offset next;

        if (way == std::ios_base::beg)
            next = off;
        else if (way == std::ios_base::cur)
            next = pos_ + off;
        else if (way == std::ios_base::end)
            next = container_.size() + off - 1;
        else
            throw std::ios_base::failure("bad seek direction");

        if (next < 0 || next >= static_cast<io::stream_offset>(container_.size()))
            throw std::ios_base::failure("bad seek offset");

        pos_ = next;
        return pos_;
    }

private:
    container_type&  container_;
    typename container_type::size_type pos_;
};

}
