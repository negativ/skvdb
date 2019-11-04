#pragma once

#include <algorithm>
#include <functional>
#include <future>
#include <thread>
#include <type_traits>
#include <unordered_map>

#include "Storage.hpp"
#include "MountPoint.hpp"
#include "VirtualEntry.hpp"
#include "util/Log.hpp"
#include "util/String.hpp"
#include "util/StringPath.hpp"
#include "util/StringPathIterator.hpp"
#include "util/Unused.hpp"

namespace {

using namespace skv::util;

const Status InvalidVolumeArgumentStatus = Status::InvalidArgument("Invalid volume");

}

namespace skv::vfs {

using namespace skv::util;

template <class IIt, class OIt, class UnaryOp, class Pred>
OIt transform_future_if(IIt start, IIt stop, OIt result, Pred pred, UnaryOp op) {
    while (start != stop) {
        auto&& ret = (*start).get();

        if (pred(ret)) {
            *result = op(ret);
            ++result;
        }
        ++start;
    }

    return result;
}

template <class IIt, class OIt, class UnaryOp>
OIt transform_future(IIt start, IIt stop, OIt result, UnaryOp op) {
    while (start != stop) {
        *result = std::move(op(std::move((*start).get())));
        ++result;
        ++start;
    }

    return result;
}

template <typename F, typename VEntries>
auto spawnCall(VEntries&& ventries, F&& call) {
    using result_t = std::invoke_result_t<F, VirtualEntry>;
    using future_result_t = std::future<result_t>;

    std::vector<future_result_t> futresults;
    std::transform(std::begin(ventries), std::end(ventries),
                   std::back_inserter(futresults),
                   [call](auto&& entry) { return std::async(std::launch::async,
                                                            [call, entry]() -> result_t {
                                                                return call(entry);
                                                            }); });

    std::vector<result_t> results;
    transform_future(std::begin(futresults), std::end(futresults),
                     std::back_inserter(results),
                     [](auto&& res) { return std::move(res); });

    return  results;
}

template <typename T>
T&& id(T&& t) {
    return std::forward<T>(t);
}

using VirtualEntries = std::vector<VirtualEntry>;

struct Storage::Impl {
    std::tuple<Status, Storage::Handle> open(std::string_view path) {
        using result_t = std::tuple<Status, VirtualEntry>;
        using future_result_t = std::future<result_t>;

        std::string vpath = simplifyPath(path);
        auto [status, mountPath] = searchMountPathFor(vpath);

        if (!status.isOk())
            return {status, Storage::InvalidHandle};

        auto& index = mpoints_.get<mount::tags::ByMountPath>();
        auto [start, stop] = index.equal_range(util::to_string(mountPath));
        auto subvpath = vpath.substr(mountPath.size(), vpath.size() - mountPath.size()); // extracting subpath from vpath
        std::vector<future_result_t> results;

        std::transform(start, stop,
                       std::back_inserter(results),
                       [subvpath](auto&& entry) {
                           return std::async(std::launch::async, [entry, subvpath]() -> result_t {
                               auto volume  = entry.volume();
                               auto subpath = simplifyPath(entry.entryPath() + "/" + subvpath);
                               auto [status, handle] = volume->open(subpath);

                               if (status.isOk())
                                   return {status, VirtualEntry(subpath, volume, handle, entry.priority())};

                               return {Status::InvalidArgument("Invalid volume"), VirtualEntry{}};
                           });
                       });

        VirtualEntries openedEntries;

        transform_future_if(std::begin(results), std::end(results),
            std::back_inserter(openedEntries),
            [](const auto& t) {
                const auto& [status, unused] = t;
                return status.isOk();
            },
            [](auto&& t) {
                auto&& [unused, ventry] = t;
                return ventry;
            } );

        if (openedEntries.empty())
            return {Status::InvalidArgument("No such path"), Storage::InvalidHandle};

        return {Status::Ok(), addVirtualEntries(std::move(openedEntries))};
    }

    Status close(Storage::Handle handle) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        auto results = spawnCall(ventries,
                                 [](VirtualEntry entry) {
                                     auto volume = entry.volume();

                                     if (auto volume = entry.volume().lock(); volume)
                                         return volume->close(entry.handle()).isOk();

                                     return false;
                                 });

        removeVirtualEntries(handle);

        auto ret = std::none_of(std::cbegin(results), std::cend(results), std::logical_not<bool>());

        return ret? Status::Ok() : Status::InvalidOperation("Unable to close handle of all volumes");
    }

    std::tuple<Status, Storage::Properties> properties(Storage::Handle handle) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return {status, {}};

        auto results = spawnCall(ventries,
                                 [](VirtualEntry entry) -> std::tuple<Status, Storage::Properties> {
                                     auto volume = entry.volume();

                                     if (auto volume = entry.volume().lock(); volume)
                                         return volume->properties(entry.handle());

                                     return {Status::InvalidArgument("Invalid volume"), {}};
                                 });

        if (std::any_of(std::cbegin(results), std::cend(results),
                        [](const auto& t) { const auto& [status, unused] = t; return !status.isOk(); }))
            return {Status::InvalidArgument("Unable to fetch links from all volumes"), {}};

        Storage::Properties ret;

        for (auto&& [status, props] : results) {
            for (auto&& [prop, value] : props) {
                if (auto it = ret.find(prop); it == std::cend(ret))
                    ret.emplace(prop, value);
            }
        }

        return {Status::Ok(), ret};
    }

    std::tuple<Status, Property> property(Storage::Handle handle, std::string_view name) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return {status, {}};

        auto spawnResults = spawnCall(ventries,
                                      [name{to_string(name)}](VirtualEntry entry) -> std::tuple<Status, Property> {
                                          auto volume = entry.volume();

                                          if (auto volume = entry.volume().lock(); volume)
                                              return volume->property(entry.handle(), name);

                                          return {Status::InvalidArgument("Invalid volume"), {}};
                                      });

        decltype(spawnResults) results;
        std::copy_if(std::begin(spawnResults), std::end(spawnResults),
                     std::back_inserter(results),
                     [](const auto& t) { const auto& [status, unused] = t; return status.isOk(); });

        if (results.empty())
            return {Status::InvalidArgument("No such property"), {}};

        auto [unused, value] = results.front(); // property from volume with highest priority

        return {Status::Ok(), value};
    }

    Status setProperty(Storage::Handle handle, std::string_view name, const Property &value) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        /* Setting property in all volumes */
        auto results = spawnCall(ventries,
                                 [name{to_string(name)}, value](VirtualEntry entry) {
                                     auto volume = entry.volume();

                                     if (auto volume = entry.volume().lock(); volume)
                                         return volume->setProperty(entry.handle(), name, value).isOk();

                                     return false;
                                 });

        auto ret = std::none_of(std::cbegin(results), std::cend(results), std::logical_not<bool>());

        return ret? Status::Ok() : Status::InvalidOperation("Unable to set property on all volumes");
    }

    Status removeProperty(Storage::Handle handle, std::string_view name) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        /* Removing property in all entries */
        auto results = spawnCall(ventries,
                                 [name{to_string(name)}](VirtualEntry entry) {
                                     auto volume = entry.volume();

                                     if (auto volume = entry.volume().lock(); volume)
                                         return volume->removeProperty(entry.handle(), name).isOk();

                                     return false;
                                 });

        auto ret = std::any_of(std::cbegin(results), std::cend(results), id<bool>);

        return ret? Status::Ok() : Status::InvalidOperation("Unable to remove properties from all volumes");
    }

    std::tuple<Status, bool> hasProperty(Storage::Handle handle, std::string_view name) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return {status, {}};

        auto results = spawnCall(ventries,
                                 [name{to_string(name)}] (VirtualEntry entry) -> std::tuple<Status, bool> {
                                     auto volume = entry.volume();

                                     if (auto volume = entry.volume().lock(); volume)
                                         return volume->hasProperty(entry.handle(), name);

                                     return {Status::InvalidArgument("Invalid volume"), {}};
                                 });

        bool hasProp = false;

        /* checking with side-effect */
        if (std::any_of(std::cbegin(results), std::cend(results),
                        [&hasProp](const auto& t) {
                            const auto& [status, v] = t;
                            hasProp = hasProp || v;

                            return !status.isOk();
                        }))
            return {Status::InvalidArgument("Unable to check property on all volumes"), {}};

        if (results.empty())
            return {Status::InvalidArgument("Unknown error"), {}};

        return {Status::Ok(), hasProp};
    }

    Status expireProperty(Storage::Handle handle, std::string_view name, chrono::system_clock::time_point tp) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        /* Expiring property in all entries */
        auto results = spawnCall(ventries,
                                 [name{to_string(name)}, tp](VirtualEntry entry) {
                                     auto volume = entry.volume();

                                     if (auto volume = entry.volume().lock(); volume)
                                         return volume->expireProperty(entry.handle(), name, tp).isOk();

                                     return false;
                                 });

        auto ret = std::any_of(std::cbegin(results), std::cend(results), id<bool>);

        return ret? Status::Ok() : Status::InvalidOperation("Unable to expire property on all volumes");
    }

    Status cancelPropertyExpiration(Storage::Handle handle, std::string_view name) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        /* Canceling property expiration in all entries */
        auto results = spawnCall(ventries,
                                 [name{to_string(name)}](VirtualEntry entry) {
                                     auto volume = entry.volume();

                                     if (auto volume = entry.volume().lock(); volume)
                                         return volume->cancelPropertyExpiration(entry.handle(), name).isOk();

                                     return false;
                                 });

        auto ret = std::any_of(std::cbegin(results), std::cend(results), id<bool>);

        return ret? Status::Ok() : Status::InvalidOperation("Unable to cancel property expiration on all volumes");
    }

    std::tuple<Status, Storage::Links> links(Storage::Handle handle) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return {status, {}};

        auto results = spawnCall(ventries,
                                 [](VirtualEntry entry) -> std::tuple<Status, Storage::Links> {
                                     auto volume = entry.volume();

                                     if (auto volume = entry.volume().lock(); volume)
                                         return volume->links(entry.handle());

                                     return {Status::InvalidArgument("Invalid volume"), {}};
                                 });

        if (std::any_of(std::cbegin(results), std::cend(results),
                        [](const auto& t) { const auto& [status, unused] = t; return !status.isOk(); }))
            return {Status::InvalidArgument("Unable to fetch links from all volumes"), {}};

        Storage::Links ret;

        for (auto&& [status, ls] : results) {
            for (auto&& l : ls) {
                if (auto it = ret.find(l); it == std::cend(ret))
                    ret.insert(l);
            }
        }

        return {Status::Ok(), ret};
    }

    Status link(Storage::Handle handle, std::string_view name) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        /* Canceling property expiration in all entries */
        auto results = spawnCall(ventries,
                                 [name{to_string(name)}](VirtualEntry entry) {
                                     auto volume = entry.volume();

                                     if (auto volume = entry.volume().lock(); volume)
                                         return volume->link(entry.handle(), name).isOk();

                                     return false;
                                 });

        auto ret = std::any_of(std::cbegin(results), std::cend(results), id<bool>);

        return ret? Status::Ok() : Status::InvalidOperation("Unable to create link");
    }

    Status unlink(Handle handle, std::string_view name) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        /* Canceling property expiration in all entries */
        auto results = spawnCall(ventries,
                                 [name{to_string(name)}](VirtualEntry entry) {
                                     auto volume = entry.volume();

                                     if (auto volume = entry.volume().lock(); volume)
                                         return volume->unlink(entry.handle(), name).isOk();

                                     return false;
                                 });

        auto ret = std::any_of(std::cbegin(results), std::cend(results), id<bool>);

        return ret? Status::Ok() : Status::InvalidOperation("Unable to remove link");
    }

    std::tuple<Status, std::string> searchMountPathFor(std::string_view path) const {
        auto searchPath = simplifyPath(path);
        ReverseStringPathIterator start{searchPath}, stop{};

        auto& index = mpoints_.get<mount::tags::ByMountPath>();

        while (start != stop) {
            auto it = index.find(*start);

            if (it != std::cend(index))
                return {Status::Ok(), it->mountPath()};

            ++start;
        }

        return {Status::NotFound("Unable to find mount point"), {}};
    }

    Status mount(IVolumePtr volume, std::string_view entryPath, std::string_view mountPath, Storage::Priority prio) {
        if (!volume)
            return InvalidVolumeArgumentStatus;

        if (!volume->claim(this).isOk())
            return Status::InvalidOperation("Volume claimed by another instance of storage");

        mount::Entry entry(mountPath, entryPath, volume, prio);

        if (!entry.open()) {
            SKV_UNUSED(volume->release(this));

            return Status::InvalidArgument("Unable to create mount point entry. Check volume is properly initialized and entry path exists.");
        }

        // TODO: protect by mutex

        auto& index = mpoints_.get<mount::tags::ByAll>();
        auto retp = index.insert(std::move(entry));

        if (!retp.second) {
            SKV_UNUSED(volume->release(this));
            entry.close();

            return Status::InvalidArgument("Mount point entry already exist");
        }

        return Status::Ok();
    }

    Status unmount(IVolumePtr volume, std::string_view entryPath, std::string_view mountPath) {
        if (!volume)
            return InvalidVolumeArgumentStatus;

        // TODO: protect by mutex

        auto& index = mpoints_.get<mount::tags::ByMountPath>();
        auto [start, stop] = index.equal_range(util::to_string(mountPath));
        auto entryIt = std::find_if(start, stop,
                                    [&](auto&& entry) {
                                        return entry.volume() == volume && entry.entryPath() == entryPath;
                                    });

        if (entryIt == std::end(index))
            return Status::InvalidArgument("No such mount point entry");

        if (!volume->release(this).isOk())
            return Status::Fatal("Unable to release volume");

        mount::Entry e(*entryIt);
        e.close();

        index.erase(entryIt);

        return Status::Ok();
    }

    Storage::Handle addVirtualEntries(VirtualEntries&& ventries) {
        // TODO: lock
        auto handle = newHandle();

        std::sort(std::begin(ventries), std::end(ventries), std::greater<VirtualEntry>()); //sort from high priority to low

        if (ventries_.emplace(handle, ventries).second)
            return handle;

        return Storage::InvalidHandle;
    }

    std::tuple<Status, VirtualEntries> getVirtualEntries(Storage::Handle handle) const {
        // TODO: lock
        auto it = ventries_.find(handle);

        if (it == std::cend(ventries_))
            return {Status::InvalidArgument("No such handle"), {}};

        return {Status::Ok(), it->second};
    }

    void removeVirtualEntries(Storage::Handle handle) {
        // TODO: lock
        auto it = ventries_.find(handle);

        if (it != std::end(ventries_))
            ventries_.erase(it);
    }

    Storage::Handle newHandle() noexcept {
        return currentHandle_.fetch_add(1);
    }

    mount::Points mpoints_{};
    std::unordered_map<Storage::Handle, VirtualEntries> ventries_;
    std::atomic<Storage::Handle> currentHandle_{IVolume::RootHandle + 1};
};

}
