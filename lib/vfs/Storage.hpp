#pragma once

#include <memory>

#include "IVolume.hpp"

namespace skv::vfs {

using namespace skv::util;

class Storage final {
    struct Impl;

public:
    using Handle     = IVolume::Handle;
    using Properties = IVolume::Properties;
    using Links      = IVolume::Links;
    using Clock      = IVolume::Clock;

    static constexpr Handle InvalidHandle = IVolume::InvalidHandle;

    Storage();
    ~Storage() noexcept;

    Storage(const Storage&) = delete;
    Storage& operator=(const Storage&) = delete;

    /**
     * @brief Open volume entry
     * @param path - virtual path
     * @return {Status::Ok(), handle} on success
     */
    [[nodiscard]] std::tuple<Status, Handle> open(std::string_view path);

    /**
     * @brief Close opened volume entry
     * @param handle - entry
     * @return Status::Ok() on success
     */
    [[nodiscard]] Status close(Handle handle);


    /**
     * @brief Get all entry properties
     * @param handle - entry
     * @return {Status::Ok(), Properties} on success
     */
    [[nodiscard]] std::tuple<Status, Properties> properties(Handle handle);

    /**
     * @brief Get specified property
     * @param handle - entry
     * @param propName - property name
     * @return {Status::Ok(), property} on success
     */
    [[nodiscard]] std::tuple<Status, Property> property(Handle handle, std::string_view propName);

    /**
     * @brief Set specified property
     * @param handle - entry
     * @param propName - property name
     * @param value - property value
     * @return Status::Ok() on success
     */
    [[nodiscard]] Status setProperty(Handle handle, std::string_view propName, const Property& value);

    /**
     * @brief Remove specified property
     * @param handle - entry
     * @param propName - property name
     * @return Status::Ok() on success
     */
    [[nodiscard]] Status removeProperty(Handle handle, std::string_view propName);

    /**
     * @brief Remove specified property exists
     * @param handle - entry
     * @param propName - property name
     * @return {Status::Ok(), exists flag} on success
     */
    [[nodiscard]] std::tuple<Status, bool> hasProperty(Handle handle, std::string_view propName);

    /**
     * @brief Remove property at specified time point
     * @param handle - entry
     * @param propName - property name
     * @param tp - time point
     * @return Status::Ok() on success
     */
    [[nodiscard]] Status expireProperty(Handle handle, std::string_view propName, Clock::time_point tp);

    /**
     * @brief Cancel property expiration
     * @param handle - entry
     * @param propName - property name
     * @return Status::Ok() on success
     */
    [[nodiscard]] Status cancelPropertyExpiration(Handle handle, std::string_view propName);


    /**
     * @brief Get linked entries names
     * @param handle - entry
     * @return {Status::Ok(), links} on success
     */
    [[nodiscard]] std::tuple<Status, Links> links(Handle handle);

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

private:
    std::unique_ptr<Impl> impl_;
};

}

