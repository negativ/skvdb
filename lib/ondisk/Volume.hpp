#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <tuple>

#include "Property.hpp"
#include "util/Status.hpp"

namespace skv::ondisk {

using namespace skv::util;
namespace chrono = std::chrono;

class Volume final {
    struct Impl;

public:
    using Handle = std::uint64_t;

    static constexpr Handle InvalidHandle = 0;

    Volume();
    ~Volume() noexcept;

    Volume(const Volume&) = delete;
    Volume& operator=(const Volume&) = delete;

    Volume(Volume&&) = delete;
    Volume& operator=(Volume&&) = delete;

    Status initialize(std::string_view directory, std::string_view volumeName);
    Status deinitialize();
    bool initialized() const noexcept;

    std::tuple<Status, Handle> open(std::string_view path);
    Status close(Handle h);

    std::tuple<Status, std::set<std::string>> children(Handle h);

    std::tuple<Status, std::set<std::string>> properties(Handle h);
    std::tuple<Status, Property> property(Handle h, std::string_view name);
    Status setProperty(Handle h, std::string_view name, const Property& value);
    Status removeProperty(Handle h, std::string_view name);
    std::tuple<Status, bool> hasProperty(Handle h, std::string_view name);
    Status expireProperty(Handle h, std::string_view name, chrono::system_clock::time_point tp);
    Status cancelPropertyExpiration(Handle h, std::string_view name);

    std::tuple<Status, Handle> link(Handle h, std::string_view name);
    Status unlink(std::string_view path);
private:
    std::unique_ptr<Impl> impl_;
};

}
