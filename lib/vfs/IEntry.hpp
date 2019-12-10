#pragma once

#include <chrono>
#include <cstdint>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>

#include "Property.hpp"
#include "util/Status.hpp"

namespace skv::vfs {

using namespace skv::util;

/**
 * @brief VFS Storage entry
 */
class IEntry {
public:
    using Handle        = std::uint64_t;
    using Properties    = std::unordered_map<std::string, Property>;

    IEntry() noexcept = default;

    IEntry(const IEntry&) = delete;
    IEntry(IEntry&&) = delete;

    IEntry& operator=(const IEntry&) = delete;
    IEntry& operator=(IEntry&&) = delete;


    /**
     * @brief handle Internal handle of item
     * @return
     */
    [[nodiscard]] virtual Handle handle() const noexcept = 0;


    /**
     * @brief hasProperty Check proeprty existance
     * @param prop Property name
     * @return
     */
    [[nodiscard]] virtual std::tuple<Status, bool> hasProperty(const std::string& prop) const noexcept = 0;

    /**
     * @brief setProperty Set property value
     * @param prop Property name
     * @param value Property value
     * @return
     */
    virtual Status setProperty(const std::string& prop, const Property& value) = 0;

    /**
     * @brief property Retrieve property value
     * @param prop Property name
     * @return
     */
    [[nodiscard]] virtual std::tuple<Status, Property> property(const std::string& prop) const = 0;

    /**
     * @brief removeProperty Remove specified property
     * @param prop Property name
     * @return
     */
    virtual Status removeProperty(const std::string& prop) = 0;

    /**
     * @brief properties Get all properties
     * @return
     */
    [[nodiscard]] virtual std::tuple<Status, Properties> properties() const  = 0;

    /**
     * @brief propertiesNames Get all properties names
     * @return
     */
    [[nodiscard]] virtual std::tuple<Status, std::set<std::string>> propertiesNames() const  = 0;


    /**
     * @brief expireProperty Remove specified property after some period
     * @param prop Property name
     * @param ms Expiration period
     * @return
     */
    virtual Status expireProperty(const std::string& prop, std::chrono::milliseconds ms) = 0;

    /**
     * @brief cancelPropertyExpiration Cancel property expiration
     * @param prop Property name
     * @return
     */
    virtual Status cancelPropertyExpiration(const std::string& prop) = 0;


    /**
     * @brief children Retrieve names of child entries
     * @return
     */
    [[nodiscard]] virtual std::tuple<Status, std::set<std::string>> links() const = 0;

protected:
    virtual ~IEntry() noexcept = default;
};

}
