#pragma once

#include <memory>
#include <string>

#include "IVolume.hpp"

namespace skv::vfs::mount {

class Entry final {
    struct Impl;

public:
    Entry(std::string_view mountPath, std::string_view entryPath, IVolumePtr volume);
    ~Entry() noexcept;

    Entry(const Entry& other);
    Entry& operator=(const Entry& other);

    Entry(Entry&& other) noexcept;
    Entry& operator=(Entry&& other) noexcept;

    /**
     * @brief Path of entry in volume
     * @return
     */
    std::string mountPath() const;

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

