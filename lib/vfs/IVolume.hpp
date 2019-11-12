#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>

#include "Property.hpp"
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
    using Properties        = std::unordered_map<std::string, vfs::Property>;
    using PropertiesNames   = std::set<std::string>;
    using Links             = std::set<std::string>;
    using Clock             = chrono::system_clock;

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
    [[nodiscard]] virtual Status initialize(std::string_view directory, std::string_view volumeName) = 0;

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
     * @brief Open volume entry
     * @param path - virtual path
     * @return {Status::Ok(), handle} on success
     */
    [[nodiscard]] virtual std::tuple<Status, Handle> open(std::string_view path) = 0;

    /**
     * @brief Close opened volume entry
     * @param handle - entry
     * @return Status::Ok() on success
     */
    [[nodiscard]] virtual Status close(Handle handle) = 0;


    /**
     * @brief Get all entry properties
     * @param handle - entry
     * @return {Status::Ok(), Properties} on success
     */
    [[nodiscard]] virtual std::tuple<Status, Properties> properties(Handle handle) = 0;

    /**
     * @brief Get names of all properties in entry
     * @param handle - entry
     * @return {Status::Ok(), PropertiesNames} on success
     */
    [[nodiscard]] virtual std::tuple<Status, PropertiesNames> propertiesNames(Handle handle) = 0;

    /**
     * @brief Get specified property
     * @param handle - entry
     * @param propName - property name
     * @return {Status::Ok(), property} on success
     */
    [[nodiscard]] virtual std::tuple<Status, Property> property(Handle handle, std::string_view propName) = 0;

    /**
     * @brief Set specified property
     * @param handle - entry
     * @param propName - property name
     * @param value - property value
     * @return Status::Ok() on success
     */
    [[nodiscard]] virtual Status setProperty(Handle handle, std::string_view propName, const Property& value) = 0;

    /**
     * @brief Remove specified property
     * @param handle - entry
     * @param propName - property name
     * @return Status::Ok() on success
     */
    [[nodiscard]] virtual Status removeProperty(Handle handle, std::string_view propName) = 0;

    /**
     * @brief Remove specified property exists
     * @param handle - entry
     * @param propName - property name
     * @return {Status::Ok(), exists flag} on success
     */
    [[nodiscard]] virtual std::tuple<Status, bool> hasProperty(Handle handle, std::string_view propName) = 0;

    /**
     * @brief Remove property at specified time point
     * @param handle - entry
     * @param propName - property name
     * @param tp - time point
     * @return Status::Ok() on success
     */
    [[nodiscard]] virtual Status expireProperty(Handle handle, std::string_view propName, Clock::time_point tp) = 0;

    /**
     * @brief Cancel property expiration
     * @param handle - entry
     * @param propName - property name
     * @return Status::Ok() on success
     */
    [[nodiscard]] virtual Status cancelPropertyExpiration(Handle handle, std::string_view propName) = 0;


    /**
     * @brief Get linked entries names
     * @param handle - entry
     * @return {Status::Ok(), links} on success
     */
    [[nodiscard]] virtual std::tuple<Status, Links> links(Handle handle) = 0;

    /**
     * @brief Create new link
     * @param handle - entry in which link will be created
     * @param name - name of created link
     * @return Status::Ok() on success
     */
    [[nodiscard]] virtual Status link(Handle h, std::string_view name) = 0;

    /**
     * @brief Remove specified link
     * @param handle - entry
     * @param name - name of link to remove
     * @return Status::Ok() on success
     */
    [[nodiscard]] virtual Status unlink(Handle h, std::string_view name) = 0;


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

using IVolumePtr    = std::shared_ptr<IVolume>;
using IVolumeWPtr   = std::weak_ptr<IVolume>;

}
