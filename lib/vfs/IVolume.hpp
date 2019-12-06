#pragma once

#include <memory>
#include <string>

#include "vfs/IEntry.hpp"
#include "util/Status.hpp"

namespace skv::vfs {

using namespace skv::util;
namespace chrono = std::chrono;

class IVolume {
public:
    using Handle = std::uint64_t;

    static constexpr Handle InvalidHandle = 0;
    static constexpr Handle RootHandle = 1;

    using Token             = void*;

    IVolume() noexcept = default;
    virtual ~IVolume() noexcept = default;

    IVolume(const IVolume&) = delete;
    IVolume& operator=(const IVolume&) = delete;

    IVolume(IVolume&&) = delete;
    IVolume& operator=(IVolume&&) = delete;

    /**
     * @brief Get entry at specified path
     * @param path - path to the entry
     * @return
     */
    [[nodiscard]] virtual std::shared_ptr<IEntry> entry(const std::string& path) = 0;

    /**
     * @brief Create new link
     * @param entry - entry in which link will be created
     * @param name - name of created link
     * @return Status::Ok() on success
     */
    [[nodiscard]] virtual Status link(IEntry& entry, const std::string& name) = 0;

    /**
     * @brief Remove specified link
     * @param entry - parent entry
     * @param name - name of link to remove
     * @return Status::Ok() on success
     */
    [[nodiscard]] virtual Status unlink(IEntry& entry, const std::string& name) = 0;


    /**
     * @brief Claiming by VFS. Can be called more than once
     * @param token
     * @return Status::Ok() on success
     */
    [[nodiscard]] virtual Status claim(Token token) noexcept = 0;

    /**
     * @brief Realising volume by VFS. If call count to release() eq. count of claim()'s then volume can be used by another VFS instance
     * @param token
     * @return
     */
    [[nodiscard]] virtual Status release(Token token) noexcept = 0;
};

using IVolumePtr = std::shared_ptr<IVolume>;
using IVolumeWPtr = std::weak_ptr<IVolume>;

}
