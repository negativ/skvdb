#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <tuple>

#include "vfs/Property.hpp"
#include "vfs/IVolume.hpp"

namespace skv::ondisk {

using namespace skv::util;
using namespace skv::vfs;

namespace chrono = std::chrono;

class Volume final: public vfs::IVolume {
    struct Impl;

public:
    Volume();
    ~Volume() noexcept override;

    Volume(const Volume&) = delete;
    Volume& operator=(const Volume&) = delete;

    Volume(Volume&&) = delete;
    Volume& operator=(Volume&&) = delete;

    // vfs::IVolume interface
    Status initialize(std::string_view directory, std::string_view volumeName) override;
    Status deinitialize() override;
    bool initialized() const noexcept override;

    std::tuple<Status, Handle> open(std::string_view path) override;
    Status close(Handle handle) override;

    std::tuple<Status, Properties> properties(Handle handle) override;
    std::tuple<Status, Property> property(Handle h, std::string_view name) override;
    Status setProperty(Handle h, std::string_view name, const Property& value) override;
    Status removeProperty(Handle h, std::string_view name) override;
    std::tuple<Status, bool> hasProperty(Handle h, std::string_view name) override;
    Status expireProperty(Handle h, std::string_view name, chrono::system_clock::time_point tp) override;
    Status cancelPropertyExpiration(Handle h, std::string_view name) override;

    std::tuple<Status, Links> links(Handle handle) override;
    Status link(Handle h, std::string_view name) override;
    Status unlink(Handle h, std::string_view path) override;

    Status claim(Token token) noexcept override;
    Status release(Token token) noexcept override;

private:
    std::unique_ptr<Impl> impl_;
};

}
