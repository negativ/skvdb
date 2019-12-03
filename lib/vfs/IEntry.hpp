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

    virtual ~IEntry() noexcept = default;

    virtual Handle handle() const noexcept = 0;

    virtual std::string name() const = 0;


    virtual bool hasProperty(const std::string& prop) const noexcept = 0;

    virtual Status setProperty(const std::string& prop, const Property& value) = 0;

    virtual std::tuple<Status, Property> property(const std::string& prop) const = 0;

    virtual Status removeProperty(const std::string& prop) = 0;

    virtual Properties properties() const  = 0;

    virtual std::set<std::string> propertiesNames() const  = 0;


    virtual Status expireProperty(const std::string& prop, std::chrono::milliseconds ms) = 0;

    virtual Status cancelPropertyExpiration(const std::string& prop) = 0;


    virtual std::set<std::string> children() const = 0;
};

}
