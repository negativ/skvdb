#pragma once

#include <memory>
#include <string>

#include "IVolume.hpp"

namespace skv::vfs {

class MountPointEntry final {
    struct Impl;

public:
    MountPointEntry(std::string_view entryPath, IVolumePtr volume);
    ~MountPointEntry() noexcept;

    /**
     * @brief Path of entry in volume
     * @return
     */
    std::string entryPath() const;

    /**
     * @brief Mounted volume
     * @return
     */
    IVolumePtr volume() const;

    /**
     * @brief Handle of the entry
     * @return
     */
    IVolume::Handle handle() const noexcept;

    /**
     * @brief Does mount entry valid
     * @return
     */
    bool valid() const noexcept;

private:
    std::unique_ptr<Impl> impl_;
};

}

