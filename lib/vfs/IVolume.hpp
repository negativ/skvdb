#pragma once

#include <memory>
#include <string>
#include <string_view>

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
     * @brief Initialize volume "volumeName" in directory "directory"
     * @param directory - volume location (directory)
     * @param volumeName - volume name
     * @return Status::Ok() on success
     */
    [[nodiscard]] virtual Status initialize(const std::string& directory, const std::string& volumeName) = 0;

    /**
     * @brief Deinitialize volume
     * @return
     */
    [[nodiscard]] virtual Status deinitialize() = 0;

    /**
     * @brief Check volume is initialized
     * @return
     */
    [[nodiscard]] virtual bool initialized() const noexcept  = 0;

    /**
     * @brief Get entry at specified path
     * @param path - path to the entry
     * @return
     */
    virtual std::shared_ptr<IEntry> entry(const std::string& path) = 0;

    /**
     * @brief Create new link
     * @param entry - entry in which link will be created
     * @param name - name of created link
     * @return Status::Ok() on success
     */
    [[nodiscard]] virtual Status link(IEntry& entry, std::string_view name) = 0;

    /**
     * @brief Remove specified link
     * @param entry - parent entry
     * @param name - name of link to remove
     * @return Status::Ok() on success
     */
    [[nodiscard]] virtual Status unlink(IEntry& entry, std::string_view name) = 0;


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
