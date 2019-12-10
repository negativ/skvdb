#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <tuple>

#include "os/File.hpp"
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

    Volume(Status &status) noexcept;
    Volume(Status &status, OpenOptions opts) noexcept;
    ~Volume() noexcept override;

    Volume(const Volume&) = delete;
    Volume& operator=(const Volume&) = delete;

    Volume(Volume&&) noexcept;
    Volume& operator=(Volume&&) = delete;

    [[nodiscard]] Status initialize(const os::path& directory, const std::string& volumeName);
    [[nodiscard]] Status deinitialize();

    [[nodiscard]] bool initialized() const noexcept;

    /**
     * @brief Get entry at specified path
     * @param path - path to the entry
     * @return
     */
    [[nodiscard]] std::shared_ptr<IEntry> entry(const std::string& path) override;


    /**
     * @brief Create new link
     * @param entry - entry in which link will be created
     * @param name - name of created link
     * @return Status::Ok() on success
     */
    [[nodiscard]] Status link(IEntry& entry, const std::string& name) override;

    /**
     * @brief Remove specified link
     * @param entry - parent entry
     * @param name - name of link to remove
     * @return Status::Ok() on success
     */
    [[nodiscard]] Status unlink(IEntry& entry, const std::string& name) override;


    /**
     * @brief Claiming by VFS. Can be called more than once
     * @param token
     * @return Status::Ok() on success
     */
    [[nodiscard]] Status claim(Token token) noexcept override;

    /**
     * @brief Realising volume by VFS. If call count to release() eq. count of claim()'s then volume can be used by another VFS instance
     * @param token
     * @return
     */
    [[nodiscard]] Status release(Token token) noexcept override;

private:
    std::unique_ptr<Impl> impl_;
};

}
