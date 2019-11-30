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
    struct OpenOptions {
        static constexpr double         DefaultCompactionRatio{0.6}; // 60% of blocks used, 40% wasted
        static constexpr std::uint64_t  DefaultCompactionDeviceMinSize{std::uint64_t{1024 * 1024 * 1024} * 4}; // compaction starts only if device size exceeds this value. 4GB default
        static constexpr std::uint32_t  DefaultLogDeviceBlockSize{2048}; // 2KB

        double          CompactionRatio{DefaultCompactionRatio};
        std::uint64_t   CompactionDeviceMinSize{DefaultCompactionDeviceMinSize};
        std::uint32_t   LogDeviceBlockSize{DefaultLogDeviceBlockSize};
        bool            LogDeviceCreateNewIfNotExist{true};
    };

    Volume();
    Volume(OpenOptions opts);
    ~Volume() noexcept override;

    Volume(const Volume&) = delete;
    Volume& operator=(const Volume&) = delete;

    Volume(Volume&&) = delete;
    Volume& operator=(Volume&&) = delete;

    // vfs::IVolume interface
    [[nodiscard]] Status initialize(std::string_view directory, std::string_view volumeName) override;
    [[nodiscard]] Status deinitialize() override;
    [[nodiscard]] bool initialized() const noexcept override;

    std::shared_ptr<IEntry> entry(const std::string& path) override;
    [[nodiscard]] std::tuple<Status, Handle> open(std::string_view path) override;
    [[nodiscard]] Status close(Handle handle) override;

    [[nodiscard]] std::tuple<Status, Properties> properties(Handle handle) override;
    [[nodiscard]] std::tuple<Status, PropertiesNames> propertiesNames(Handle handle) override;
    [[nodiscard]] std::tuple<Status, Property> property(Handle h, std::string_view name) override;
    [[nodiscard]] Status setProperty(Handle h, std::string_view name, const Property& value) override;
    [[nodiscard]] Status removeProperty(Handle h, std::string_view name) override;
    [[nodiscard]] std::tuple<Status, bool> hasProperty(Handle h, std::string_view name) override;
    [[nodiscard]] Status expireProperty(Handle h, std::string_view name, chrono::system_clock::time_point tp) override;
    [[nodiscard]] Status cancelPropertyExpiration(Handle h, std::string_view name) override;

    [[nodiscard]] std::tuple<Status, Links> links(Handle handle) override;
    [[nodiscard]] Status link(Handle handle, std::string_view name) override;
    [[nodiscard]] Status unlink(Handle handle, std::string_view name) override;

    [[nodiscard]] Status claim(Token token) noexcept override;
    [[nodiscard]] Status release(Token token) noexcept override;

private:
    std::unique_ptr<Impl> impl_;
};

}
