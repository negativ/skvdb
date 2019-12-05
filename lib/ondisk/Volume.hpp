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

    Volume(Volume&&) noexcept;
    Volume& operator=(Volume&&) = delete;

    [[nodiscard]] Status initialize(const std::string& directory, const std::string& volumeName);
    [[nodiscard]] Status deinitialize();

    [[nodiscard]] bool initialized() const noexcept;

    std::shared_ptr<IEntry> entry(const std::string& path) override;

    [[nodiscard]] Status link(IEntry& entry, std::string_view name) override;
    [[nodiscard]] Status unlink(IEntry& entry, std::string_view name) override;

    [[nodiscard]] Status claim(Token token) noexcept override;
    [[nodiscard]] Status release(Token token) noexcept override;

private:
    std::unique_ptr<Impl> impl_;
};

}
