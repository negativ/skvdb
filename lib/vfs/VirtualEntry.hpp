#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "IVolume.hpp"
#include "MountPointEntry.hpp"

namespace skv::vfs {

/**
 * @brief VFS virtual entry
 */
class VirtualEntry final {
    struct Impl;

public:
    using Priority = mount::Entry::Priority;

    VirtualEntry() noexcept;
    VirtualEntry(std::string_view entryPath, IVolumeWPtr volume, IVolume::Handle handle, Priority prio);
    ~VirtualEntry() noexcept;

    VirtualEntry(const VirtualEntry& other);
    VirtualEntry& operator=(const VirtualEntry& other);

    VirtualEntry(VirtualEntry&& other) noexcept;
    VirtualEntry& operator=(VirtualEntry&& other) noexcept;

    /**
     * @brief Path of entry in volume
     * @return
     */
    [[nodiscard]] std::string entryPath() const;

    /**
     * @brief Parent volume
     * @return
     */
    [[nodiscard]] IVolumeWPtr volume() const;

    /**
     * @brief Handle of the entry
     * @return
     */
    [[nodiscard]] IVolume::Handle handle() const noexcept;

    /**
     * @brief Entry priority
     * @return
     */
    [[nodiscard]] Priority priority() const noexcept;

    /**
     * @brief valid
     * @return
     */
    [[nodiscard]] bool valid() const noexcept;

    [[nodiscard]] bool operator<(const VirtualEntry& other) const noexcept;
    [[nodiscard]] bool operator>(const VirtualEntry& other) const noexcept;

private:
    std::unique_ptr<Impl> impl_;
};

}

