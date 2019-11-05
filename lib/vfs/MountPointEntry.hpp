#pragma once

#include <limits>
#include <memory>
#include <string>

#include "IVolume.hpp"

namespace skv::vfs::mount {

class Entry final {
    struct Impl;

public:
    using Priority = std::size_t;

    static constexpr Priority MaxPriority       = std::numeric_limits<Priority>::max();
    static constexpr Priority MinPriority       = std::numeric_limits<Priority>::min();
    static constexpr Priority DefaultPriority   = MinPriority + 1;

    Entry(std::string_view mountPath, std::string_view entryPath, IVolumePtr volume, Priority prio = DefaultPriority);
    ~Entry() noexcept;

    Entry(const Entry& other);
    Entry& operator=(const Entry& other);

    Entry(Entry&& other) noexcept;
    Entry& operator=(Entry&& other) noexcept;

    /**
     * @brief Path of entry in volume
     * @return
     */
    [[nodiscard]] std::string mountPath() const;

    /**
     * @brief Path of entry in volume
     * @return
     */
    [[nodiscard]] std::string entryPath() const;

    /**
     * @brief Mounted volume
     * @return
     */
    [[nodiscard]] IVolumePtr volume() const;

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
     * @brief Trying to open entry
     * @return
     */
    [[nodiscard]] bool open();

    /**
     * @brief Closes entry
     */
    void close();

    /**
     * @brief opened
     * @return
     */
    [[nodiscard]] bool opened() const noexcept;

private:
    std::unique_ptr<Impl> impl_;
};

}

