#pragma once

#include <chrono>
#include <cstdint>
#include <set>
#include <string>
#include <tuple>

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

    IVolume();
    virtual ~IVolume() noexcept;

    IVolume(const IVolume&) = delete;
    IVolume& operator=(const IVolume&) = delete;

    IVolume(IVolume&&) = delete;
    IVolume& operator=(IVolume&&) = delete;

    /**
     * @brief initialize
     * @param directory
     * @param volumeName
     * @return
     */
    virtual Status initialize(std::string_view directory, std::string_view volumeName) = 0;

    /**
     * @brief deinitialize
     * @return
     */
    virtual Status deinitialize() = 0;

    /**
     * @brief initialized
     * @return
     */
    virtual bool initialized() const noexcept  = 0;


    /**
     * @brief open
     * @param path
     * @return
     */
    virtual std::tuple<Status, Handle> open(std::string_view path) = 0;

    /**
     * @brief close
     * @param h
     * @return
     */
    virtual Status close(Handle h) = 0;


    /**
     * @brief properties
     * @param h
     * @return
     */
    virtual std::tuple<Status, std::set<std::string>> properties(Handle h) = 0;

    /**
     * @brief property
     * @param h
     * @param name
     * @return
     */
    virtual std::tuple<Status, Property> property(Handle h, std::string_view name) = 0;

    /**
     * @brief setProperty
     * @param h
     * @param name
     * @param value
     * @return
     */
    virtual Status setProperty(Handle h, std::string_view name, const Property& value) = 0;

    /**
     * @brief removeProperty
     * @param h
     * @param name
     * @return
     */
    virtual Status removeProperty(Handle h, std::string_view name) = 0;

    /**
     * @brief hasProperty
     * @param h
     * @param name
     * @return
     */
    virtual std::tuple<Status, bool> hasProperty(Handle h, std::string_view name) = 0;

    /**
     * @brief expireProperty
     * @param h
     * @param name
     * @param tp
     * @return
     */
    virtual Status expireProperty(Handle h, std::string_view name, chrono::system_clock::time_point tp) = 0;

    /**
     * @brief cancelPropertyExpiration
     * @param h
     * @param name
     * @return
     */
    virtual Status cancelPropertyExpiration(Handle h, std::string_view name) = 0;


    /**
     * @brief links
     * @param h
     * @return
     */
    virtual std::tuple<Status, std::set<std::string>> links(Handle h) = 0;

    /**
     * @brief link
     * @param h
     * @param name
     * @return
     */
    virtual Status link(Handle h, std::string_view name) = 0;

    /**
     * @brief unlink
     * @param h
     * @param path
     * @return
     */
    virtual Status unlink(Handle h, std::string_view path) = 0;
};

}
