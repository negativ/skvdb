#pragma once

#include <limits>
#include <memory>

#include "IVolume.hpp"
#include "MountPointEntry.hpp"

namespace skv::vfs {

using namespace skv::util;

class Storage final {
    struct Impl;

public:
    using Handle   = IVolume::Handle;
    using Priority  = mount::Entry::Priority;

    static constexpr Handle   InvalidHandle     = IVolume::InvalidHandle;
    static constexpr Priority MaxPriority       = mount::Entry::MaxPriority;
    static constexpr Priority MinPriority       = mount::Entry::MinPriority;
    static constexpr Priority DefaultPriority   = mount::Entry::DefaultPriority;

    Storage();
    ~Storage() noexcept;

    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;

    Storage(Storage&&) noexcept;
    Storage& operator=(Storage&&) noexcept = delete;

    /**
     * @brief Create new link
     * @param handle - entry in which link will be created
     * @param name - name of created link
     * @return Status::Ok() on success
     */
    [[nodiscard]] Status link(Handle h, std::string_view name);

    /**
     * @brief Remove specified link
     * @param handle - entry
     * @param name - name of link to remove
     * @return Status::Ok() on success
     */
    [[nodiscard]] Status unlink(Handle h, std::string_view name);


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

