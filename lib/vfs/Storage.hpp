#pragma once

#include <limits>
#include <memory>

#include "IVolume.hpp"
#include "MountPointEntry.hpp"

namespace skv::vfs {

using namespace skv::util;

class Storage final: public IVolume {
    struct Impl;

public:
    using Priority  = mount::Entry::Priority;

    static constexpr Handle   InvalidHandle     = IVolume::InvalidHandle;
    static constexpr Priority MaxPriority       = mount::Entry::MaxPriority;
    static constexpr Priority MinPriority       = mount::Entry::MinPriority;
    static constexpr Priority DefaultPriority   = mount::Entry::DefaultPriority;

    Storage();
    ~Storage() noexcept override;

    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;

    Storage(Storage&&) noexcept;
    Storage& operator=(Storage&&) noexcept = delete;

    std::shared_ptr<IEntry> entry(const std::string &path) override;

    Status link(IEntry &entry, std::string_view name) override;

    Status unlink(IEntry &entry, std::string_view name) override;

    Status claim(Token token) noexcept override;

    Status release(Token token) noexcept override;

    /**
     * @brief Mount volume entry path to VFS storage
     * @param volume - volume that contains entry path
     * @param entryPath - entry path to mount
     * @param mountPath - path in VFS where entry would be mounted
     * @param prio - priority of mount point
     * @return Status::Ok() on success
     */
    [[nodiscard]] Status mount(const IVolumePtr& volume, std::string_view entryPath, std::string_view mountPath, Priority prio = DefaultPriority);

    /**
     * @brief Unmount volume entry path from VFS storage
     * @param volume - volume that contains entry paths
     * @param entryPath - entry path to unmount
     * @param mountPath - mount point path
     * @return Status::Ok() on success
     */
    [[nodiscard]] Status unmount(const IVolumePtr& volume, std::string_view entryPath, std::string_view mountPath);

private:
    std::unique_ptr<Impl> impl_;
};

}

