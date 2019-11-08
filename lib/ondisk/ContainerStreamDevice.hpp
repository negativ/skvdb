#pragma once

#include <algorithm>
#include <functional>
#include <iostream>
#include <vector>

#include <boost/iostreams/categories.hpp>
#include <boost/iostreams/positioning.hpp>

namespace skv::ondisk {

namespace io = boost::iostreams;

template<typename Container = std::vector<char>>
class ContainerStreamDevice final {
public:
    using container_type    = std::decay_t<Container>;
    using char_type         = typename Container::value_type;
    using category          = io::seekable_device_tag;

    ContainerStreamDevice(Container& container) noexcept
        : container_(std::ref(container)), pos_(0)
    { }

    ~ContainerStreamDevice() noexcept = default;

    ContainerStreamDevice(const ContainerStreamDevice&) = default;
    ContainerStreamDevice& operator=(const ContainerStreamDevice&) = default;

    ContainerStreamDevice(ContainerStreamDevice&&) noexcept = delete;
    ContainerStreamDevice& operator=(ContainerStreamDevice&&) noexcept = delete;

    [[nodiscard]] std::streamsize read(char_type* s, std::streamsize n) {
        auto& container = container_.get();
        auto amt = static_cast<std::streamsize>(container.size() - pos_);
        auto result = std::min(n, amt);

        if (result != 0) {
            std::copy(std::next(std::cbegin(container), pos_),
                      std::next(std::cbegin(container), pos_ + result),
                      s);

            pos_ += result;

            return result;
        }

        return -1;
    }

    [[nodiscard]] std::streamsize write(const char_type* s, std::streamsize n) {
        std::streamsize result = 0;
        auto& container = container_.get();

        if (pos_ != container.size()) {
            auto amt = static_cast<std::streamsize>(container.size() - pos_);
            result = std::min(n, amt);

            std::copy(s, s + result,
                      std::next(std::begin(container), pos_));

            pos_ += result;
        }

        if (result < n) {
            container.insert(std::end(container), std::next(s, result), std::next(s, n));
            pos_ = container.size();
        }

        return n;
    }

    [[nodiscard]] io::stream_offset seek(io::stream_offset off, std::ios_base::seekdir way) {
        io::stream_offset next;
        auto& container = container_.get();

        if (way == std::ios_base::beg)
            next = off;
        else if (way == std::ios_base::cur)
            next = pos_ + off;
        else if (way == std::ios_base::end)
            next = container.size() + off - 1;
        else
            throw std::ios_base::failure("bad seek direction");

        if (next < 0 || next >= static_cast<io::stream_offset>(container.size()))
            throw std::ios_base::failure("bad seek offset");

        pos_ = next;
        return pos_;
    }

private:
    std::reference_wrapper<container_type> container_;
    typename container_type::size_type pos_;
};

}
