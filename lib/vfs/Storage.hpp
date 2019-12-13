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

    Storage(Status& status) noexcept;
    ~Storage() noexcept override;

    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;

    Storage(Storage&&) noexcept;
    Storage& operator=(Storage&&) noexcept = delete;

    /**
     * @brief Get entry at specified path
     * @param path - path to the entry
     * @return
     */
    [[nodiscard]] std::shared_ptr<IEntry> entry(const std::string &path) override;

    /**
     * @brief Create new link
     * @param entry - entry in which link will be created
     * @param name - name of created link
     * @return Status::Ok() on success
     */
    [[nodiscard]] Status link(IEntry &entry, const std::string& name) override;

    /**
     * @brief Remove specified link
     * @param entry - parent entry
     * @param name - name of link to remove
     * @return Status::Ok() on success
     */
    [[nodiscard]] Status unlink(IEntry &entry, const std::string& name) override;

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

    /**
     * @brief Mount volume entry path to VFS storage
     * @param volume - volume that contains entry path
     * @param entryPath - entry path to mount
     * @param mountPath - path in VFS where entry would be mounted
     * @param prio - priority of mount point
     * @return Status::Ok() on success
     */
    [[nodiscard]] Status mount(const std::shared_ptr<IVolume>& volume, const std::string& entryPath, const std::string& mountPath, Priority prio = DefaultPriority);

    /**
     * @brief Unmount volume entry path from VFS storage
     * @param volume - volume that contains entry paths
     * @param entryPath - entry path to unmount
     * @param mountPath - mount point path
     * @return Status::Ok() on success
     */
    [[nodiscard]] Status unmount(const std::shared_ptr<IVolume>& volume, const std::string& entryPath, const std::string& mountPath);

private:
    std::unique_ptr<Impl> impl_;
};

}

