#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>

#include "Storage.hpp"
#include "MountPoint.hpp"
#include "VirtualEntry.hpp"
#include "util/Status.hpp"
#include "util/String.hpp"
#include "util/StringPath.hpp"
#include "util/StringPathIterator.hpp"
#include "util/ThreadPool.hpp"
#include "util/Unused.hpp"

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

template <typename T>
T&& id(T&& t) {
    return std::forward<T>(t);
}

using VirtualEntries = std::vector<VirtualEntry>;

struct Storage::Impl {
    const Status InvalidVolumeArgumentStatus = Status::InvalidArgument("Invalid volume");

    template <typename F, typename VEntries>
    auto spawnCall(VEntries&& ventries, F&& call) {
        using namespace std::literals;

        using result_t = std::invoke_result_t<F, VirtualEntry>;
        using future_result_t = std::future<result_t>;

        std::vector<future_result_t> futresults;
        futresults.reserve(ventries.size());

        std::transform(std::begin(ventries), std::end(ventries),
                       std::back_inserter(futresults),
                       [this, call](auto&& entry) {
                           return threadPool_.schedule([call, entry]() -> result_t {
                               return call(entry);
                           });
                       });


        while (!std::all_of(std::begin(futresults),
                            std::end(futresults),
                            [](auto& f) {
                                return (f.wait_for(0ms) == std::future_status::ready);
                            }))
            threadPool_.throttle();

        std::vector<result_t> results;
        results.reserve(ventries.size());

        transform_future(std::begin(futresults), std::end(futresults),
                         std::back_inserter(results),
                         [](auto&& res) { return std::forward<decltype(res)>(res); });

        return  results;
    }

    Impl() = default;
    ~Impl() noexcept = default;

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    Impl(Impl&&) noexcept = delete;
    Impl& operator=(Impl&&) noexcept = delete;

    [[nodiscard]] std::tuple<Status, Storage::Handle> open(std::string_view path) {
        using result_t = std::tuple<Status, VirtualEntry>;
        using future_result_t = std::future<result_t>;

        std::string vpath = simplifyPath(path);
        auto [status, mountPath, mountEntries] = searchMountPathFor(vpath);

        if (!status.isOk())
            return {status, Storage::InvalidHandle};

        auto subvpath = vpath.substr(mountPath.size(), vpath.size() - mountPath.size()); // extracting subpath from vpath
        std::vector<future_result_t> results;

        std::transform(std::cbegin(mountEntries), std::cend(mountEntries),
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

    [[nodiscard]] Status close(Storage::Handle handle) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        auto results = spawnCall(ventries,
                                 [](const auto& entry) {
                                     auto volume = entry.volume();

                                     if (auto volume = entry.volume().lock(); volume)
                                         return volume->close(entry.handle()).isOk();

                                     return false;
                                 });

        removeVirtualEntries(handle);

        auto ret = std::none_of(std::cbegin(results), std::cend(results), std::logical_not<bool>());

        return ret? Status::Ok() : Status::InvalidOperation("Unable to close handle of all volumes");
    }

    [[nodiscard]] std::tuple<Status, Storage::Properties> properties(Storage::Handle handle) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return {status, {}};

        auto results = spawnCall(ventries,
                                 [](const auto& entry) -> std::tuple<Status, Storage::Properties> {
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

    [[nodiscard]] std::tuple<Status, Property> property(Storage::Handle handle, std::string_view name) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return {status, {}};

        auto spawnResults = spawnCall(ventries,
                                      [name{to_string(name)}](const auto& entry) -> std::tuple<Status, Property> {
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

    [[nodiscard]] Status setProperty(Storage::Handle handle, std::string_view name, const Property &value) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        /* Setting property in all volumes */
        auto results = spawnCall(ventries,
                                 [name{to_string(name)}, value](const auto& entry) {
                                     auto volume = entry.volume();

                                     if (auto volume = entry.volume().lock(); volume)
                                         return volume->setProperty(entry.handle(), name, value).isOk();

                                     return false;
                                 });

        auto ret = std::none_of(std::cbegin(results), std::cend(results), std::logical_not<bool>());

        return ret? Status::Ok() : Status::InvalidOperation("Unable to set property on all volumes");
    }

    [[nodiscard]] Status removeProperty(Storage::Handle handle, std::string_view name) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        /* Removing property in all entries */
        auto results = spawnCall(ventries,
                                 [name{to_string(name)}](const auto& entry) {
                                     auto volume = entry.volume();

                                     if (auto volume = entry.volume().lock(); volume)
                                         return volume->removeProperty(entry.handle(), name).isOk();

                                     return false;
                                 });

        auto ret = std::any_of(std::cbegin(results), std::cend(results), id<bool>);

        return ret? Status::Ok() : Status::InvalidOperation("Unable to remove properties from all volumes");
    }

    [[nodiscard]] std::tuple<Status, bool> hasProperty(Storage::Handle handle, std::string_view name) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return {status, {}};

        auto results = spawnCall(ventries,
                                 [name{to_string(name)}] (const auto& entry) -> std::tuple<Status, bool> {
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

    [[nodiscard]] Status expireProperty(Storage::Handle handle, std::string_view name, chrono::system_clock::time_point tp) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        /* Expiring property in all entries */
        auto results = spawnCall(ventries,
                                 [name{to_string(name)}, tp](const auto& entry) {
                                     auto volume = entry.volume();

                                     if (auto volume = entry.volume().lock(); volume)
                                         return volume->expireProperty(entry.handle(), name, tp).isOk();

                                     return false;
                                 });

        auto ret = std::any_of(std::cbegin(results), std::cend(results), id<bool>);

        return ret? Status::Ok() : Status::InvalidOperation("Unable to expire property on all volumes");
    }

    [[nodiscard]] Status cancelPropertyExpiration(Storage::Handle handle, std::string_view name) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        /* Canceling property expiration in all entries */
        auto results = spawnCall(ventries,
                                 [name{to_string(name)}](const auto& entry) {
                                     auto volume = entry.volume();

                                     if (auto volume = entry.volume().lock(); volume)
                                         return volume->cancelPropertyExpiration(entry.handle(), name).isOk();

                                     return false;
                                 });

        auto ret = std::any_of(std::cbegin(results), std::cend(results), id<bool>);

        return ret? Status::Ok() : Status::InvalidOperation("Unable to cancel property expiration on all volumes");
    }

    [[nodiscard]] std::tuple<Status, Storage::Links> links(Storage::Handle handle) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return {status, {}};

        auto results = spawnCall(ventries,
                                 [](const auto& entry) -> std::tuple<Status, Storage::Links> {
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

    [[nodiscard]] Status link(Storage::Handle handle, std::string_view name) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        /* Canceling property expiration in all entries */
        auto results = spawnCall(ventries,
                                 [name{to_string(name)}](const auto& entry) {
                                     auto volume = entry.volume();

                                     if (auto volume = entry.volume().lock(); volume)
                                         return volume->link(entry.handle(), name).isOk();

                                     return false;
                                 });

        auto ret = std::any_of(std::cbegin(results), std::cend(results), id<bool>);

        return ret? Status::Ok() : Status::InvalidOperation("Unable to create link");
    }

    [[nodiscard]] Status unlink(Handle handle, std::string_view name) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        /* Canceling property expiration in all entries */
        auto results = spawnCall(ventries,
                                 [name{to_string(name)}](const auto& entry) {
                                     auto volume = entry.volume();

                                     if (auto volume = entry.volume().lock(); volume)
                                         return volume->unlink(entry.handle(), name).isOk();

                                     return false;
                                 });

        auto ret = std::any_of(std::cbegin(results), std::cend(results), id<bool>);

        return ret? Status::Ok() : Status::InvalidOperation("Unable to remove link");
    }

    [[nodiscard]] std::tuple<Status, std::string, std::vector<mount::Entry>> searchMountPathFor(std::string_view path) const {
        auto searchPath = simplifyPath(path);
        ReverseStringPathIterator start{searchPath},
                                  stop{};

        std::shared_lock locker(mpointsLock_);

        auto& index = mpoints_.get<mount::tags::ByMountPath>();

        while (start != stop) {
            auto mpath = *start;
            auto it = index.find(mpath);

            if (it != std::cend(index)) {
                auto [start, stop] = index.equal_range(mpath);

                std::vector<mount::Entry> ret;
                std::copy(start, stop, std::back_inserter(ret));

                return {Status::Ok(), mpath, ret};
            }

            ++start;
        }

        return {Status::NotFound("Unable to find mount point"), {}, {}};
    }

    [[nodiscard]] Status mount(const IVolumePtr& volume, std::string_view entryPath, std::string_view mountPath, Storage::Priority prio) {
        if (!volume)
            return InvalidVolumeArgumentStatus;

        if (!volume->claim(this).isOk())
            return Status::InvalidOperation("Volume claimed by another instance of VFS storage");

        mount::Entry entry(mountPath, entryPath, volume, prio);

        if (!entry.open()) {
            SKV_UNUSED(volume->release(this));

            return Status::InvalidArgument("Unable to create mount point entry. Check volume is properly initialized and entry path exists.");
        }

        std::unique_lock locker(mpointsLock_);

        auto& index = mpoints_.get<mount::tags::ByAll>();
        auto retp = index.insert(entry);

        if (!retp.second) {
            SKV_UNUSED(volume->release(this));
            entry.close();

            return Status::InvalidArgument("Mount point entry already exist");
        }

        return Status::Ok();
    }

    [[nodiscard]] Status unmount(IVolumePtr volume, std::string_view entryPath, std::string_view mountPath) {
        if (!volume)
            return InvalidVolumeArgumentStatus;

        std::unique_lock locker(mpointsLock_);

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

    [[nodiscard]] Storage::Handle addVirtualEntries(VirtualEntries&& ventries) {
        auto handle = newHandle();

        std::sort(std::begin(ventries), std::end(ventries), std::greater<VirtualEntry>()); //sort from high priority to low

        std::unique_lock locker(ventriesLock_);

        ventries_.emplace(handle, ventries);

        return handle;
    }

    [[nodiscard]] std::tuple<Status, VirtualEntries> getVirtualEntries(Storage::Handle handle) const {
        std::shared_lock locker(ventriesLock_);

        auto it = ventries_.find(handle);

        if (it == std::cend(ventries_))
            return {Status::InvalidArgument("No such handle"), {}};

        return {Status::Ok(), it->second};
    }

    void removeVirtualEntries(Storage::Handle handle) {
        std::unique_lock locker(ventriesLock_);

        auto it = ventries_.find(handle);

        if (it != std::end(ventries_))
            ventries_.erase(it);
    }

    [[nodiscard]] Storage::Handle newHandle() noexcept {
        return currentHandle_.fetch_add(1);
    }

    mount::Points mpoints_{};
    std::unordered_map<Storage::Handle, VirtualEntries> ventries_;
    std::atomic<Storage::Handle> currentHandle_{IVolume::RootHandle + 1};
    mutable std::shared_mutex mpointsLock_;
    mutable std::shared_mutex ventriesLock_;
    ThreadPool threadPool_;
};

}
