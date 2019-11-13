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
#include "util/Log.hpp"
#include "util/Status.hpp"
#include "util/String.hpp"
#include "util/StringPath.hpp"
#include "util/StringPathIterator.hpp"
#include "util/ThreadPool.hpp"
#include "util/Unused.hpp"

namespace skv::vfs {

using namespace skv::util;

using VirtualEntries = std::vector<VirtualEntry>;

struct Storage::Impl {
    const Status InvalidVolumeArgumentStatus = Status::InvalidArgument("Invalid volume");

    template <typename Iterator, typename F, typename ... Args>
    auto forEachEntry(Iterator start, Iterator stop, F&& func, Args&& ... args) {
        using namespace std::literals;
        using result      = std::invoke_result_t<F, IVolume*, IVolume::Handle, Args...>;
        using future      = std::future<result>;
        using future_list = std::vector<future>;
        using result_list = std::vector<result>;

        result_list results;
        future_list futures;

        auto dist = std::distance(start, stop);

        if (dist > 1) {
            auto it = start;

            std::advance(start, 1);// we will do the first call in current thread context

            try {
                std::for_each(start, stop,
                              [&](const auto& ventry) {
                                  if (auto volume = ventry.volume().lock(); volume)
                                      futures.emplace_back(threadPool_.schedule(std::forward<F>(func), volume.get(), ventry.handle(), std::forward<Args>(args)...));
                              });
            }
            catch (const std::exception& e) {
                Log::e("vfs::Storage", "Exception: ", e.what());
            }

            if (auto volume = it->volume().lock(); volume)
                results.emplace_back(std::invoke(std::forward<F>(func), volume.get(), it->handle(), std::forward<Args>(args)...));

            waitAllFutures(std::begin(futures), std::end(futures));

            std::transform(std::begin(futures), std::end(futures),
                           std::back_inserter(results),
                           [](auto& f) { return f.get(); });
        }
        else if (dist == 1) {
            if (auto volume = start->volume().lock(); volume)
                results.emplace_back(std::invoke(std::forward<F>(func), volume.get(), start->handle(), std::forward<Args>(args)...));
        }

        return  results;
    }

    template <typename Iterator>
    void waitAllFutures(Iterator start, Iterator stop) {
        using namespace std::literals;

        while (!std::all_of(start, stop,
                            [](auto& f) { return (f.wait_for(0ms) == std::future_status::ready); }))
            threadPool_.throttle(); // helping thread pool to do his work
    }

    Impl() = default;
    ~Impl() noexcept = default;

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    Impl(Impl&&) noexcept = delete;
    Impl& operator=(Impl&&) noexcept = delete;

    [[nodiscard]] std::tuple<Status, Storage::Handle> open(std::string_view path) {
        using result      = std::tuple<Status, VirtualEntry>;
        using future      = std::future<result>;
        using future_list = std::vector<future>;
        using result_list = std::vector<result>;

        std::string vpath = simplifyPath(path);
        auto [status, mountPath, mountEntries] = searchMountPathFor(vpath);

        if (!status.isOk())
            return {status, Storage::InvalidHandle};

        auto subvpath = vpath.substr(mountPath.size(), vpath.size() - mountPath.size()); // extracting subpath from vpath
        future_list futures;

        try {
            std::transform(std::begin(mountEntries), std::end(mountEntries),
                           std::back_inserter(futures),
                           [&](auto&& entry) {
                               return threadPool_.schedule([&]() -> std::tuple<Status, VirtualEntry> {
                                   auto volume  = entry.volume();
                                   auto subpath = simplifyPath(entry.entryPath() + "/" + subvpath);
                                   auto [status, handle] = volume->open(subpath);

                                   if (status.isOk())
                                       return {status, VirtualEntry(subpath, volume, handle, entry.priority())};

                                   return {Status::InvalidArgument("Invalid volume"), VirtualEntry{}};
                               });
                           });
        }
        catch (const std::exception &e) {
            return {Status::Fatal(e.what()), Storage::InvalidHandle};
        }

        waitAllFutures(std::begin(futures), std::end(futures));

        result_list results;

        std::transform(std::begin(futures), std::end(futures),
                       std::back_inserter(results),
                       [](auto& f) { return f.get(); });

        VirtualEntries openedEntries;

        for (const auto& [status, entry] : results) {
            if (status.isOk())
                openedEntries.emplace_back(entry);
        }

        if (openedEntries.empty())
            return {Status::InvalidArgument("No such path"), Storage::InvalidHandle};

        return {Status::Ok(), addVirtualEntries(std::move(openedEntries))};
    }

    [[nodiscard]] Status close(Storage::Handle handle) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        auto results = forEachEntry(std::cbegin(ventries), std::cend(ventries), &IVolume::close);

        removeVirtualEntries(handle);

        auto ret = std::all_of(std::cbegin(results), std::cend(results), [](const auto& s) { return s.isOk(); });

        return ret? Status::Ok() : Status::InvalidOperation("Unable to close handle of all volumes");
    }

    [[nodiscard]] std::tuple<Status, Storage::Properties> properties(Storage::Handle handle) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return {status, {}};

        auto results = forEachEntry(std::cbegin(ventries), std::cend(ventries), &IVolume::properties);

        if (std::any_of(std::cbegin(results), std::cend(results),
                        [](const auto& t) { const auto& [status, unused] = t; SKV_UNUSED(unused); return !status.isOk(); }))
            return {Status::InvalidArgument("Unable to fetch properties from all volumes"), {}};

        Storage::Properties ret;

        for (auto&& [status, props] : results) {
            SKV_UNUSED(status);

            for (auto&& [prop, value] : props) {
                if (auto it = ret.find(prop); it == std::cend(ret))
                    ret.emplace(prop, value);
            }
        }

        return {Status::Ok(), ret};
    }

    std::tuple<Status, Storage::PropertiesNames> propertiesNames(Handle handle) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return { status, {} };

        auto results = forEachEntry(std::cbegin(ventries), std::cend(ventries), &IVolume::propertiesNames);

        if (std::any_of(std::cbegin(results), std::cend(results),
            [](const auto& t) { const auto& [status, unused] = t; SKV_UNUSED(unused); return !status.isOk(); }))
            return { Status::InvalidArgument("Unable to fetch properties names from all volumes"), {} };

        Storage::PropertiesNames ret;

        for (const auto& [status, propNames] : results) {
            SKV_UNUSED(status);

            std::copy(std::cbegin(propNames), std::cend(propNames),
                      std::inserter(ret, std::begin(ret)));
        }

        return { Status::Ok(), ret };
    }

    [[nodiscard]] std::tuple<Status, Property> property(Storage::Handle handle, std::string_view name) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return {status, {}};

        auto results = forEachEntry(std::cbegin(ventries), std::cend(ventries), &IVolume::property, name);

        for (const auto& [status, value] : results) {
            if (status.isOk())
                return {status, value};
        }

        return {Status::InvalidArgument("No such property"), {}};
    }

    [[nodiscard]] Status setProperty(Storage::Handle handle, std::string_view name, const Property &value) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        auto results = forEachEntry(std::cbegin(ventries), std::cend(ventries), &IVolume::setProperty, name, value);
        auto ret = std::all_of(std::cbegin(results), std::cend(results), [](const auto& s) { return s.isOk(); });

        return ret? Status::Ok() : Status::InvalidOperation("Unable to set property on all volumes");
    }

    [[nodiscard]] Status removeProperty(Storage::Handle handle, std::string_view name) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        /* Removing property in all entries */
        auto results = forEachEntry(std::cbegin(ventries), std::cend(ventries), &IVolume::removeProperty, name);
        auto ret = std::any_of(std::cbegin(results), std::cend(results), [](const auto& s) { return s.isOk(); });

        return ret? Status::Ok() : Status::InvalidOperation("Unable to remove properties from all volumes");
    }

    [[nodiscard]] std::tuple<Status, bool> hasProperty(Storage::Handle handle, std::string_view name) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return {status, {}};

        auto results = forEachEntry(std::cbegin(ventries), std::cend(ventries), &IVolume::hasProperty, name);
        auto hasProp = false;

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
        auto results = forEachEntry(std::cbegin(ventries), std::cend(ventries), &IVolume::expireProperty, name, tp);
        auto ret = std::any_of(std::cbegin(results), std::cend(results), [](const auto& s) { return s.isOk(); });

        return ret? Status::Ok() : Status::InvalidOperation("Unable to expire property on all volumes");
    }

    [[nodiscard]] Status cancelPropertyExpiration(Storage::Handle handle, std::string_view name) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        /* Canceling property expiration in all entries */
        auto results = forEachEntry(std::cbegin(ventries), std::cend(ventries), &IVolume::cancelPropertyExpiration, name);
        auto ret = std::any_of(std::cbegin(results), std::cend(results), [](const auto& s) { return s.isOk(); });

        return ret? Status::Ok() : Status::InvalidOperation("Unable to cancel property expiration on all volumes");
    }

    [[nodiscard]] std::tuple<Status, Storage::Links> links(Storage::Handle handle) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return {status, {}};

        auto results = forEachEntry(std::cbegin(ventries), std::cend(ventries), &IVolume::links);

        if (std::any_of(std::cbegin(results), std::cend(results),
                        [](const auto& t) { const auto& [status, unused] = t; SKV_UNUSED(unused); return !status.isOk(); }))
            return {Status::InvalidArgument("Unable to fetch links from all volumes"), {}};

        Storage::Links ret;

        for (auto&& [status, ls] : results) {
            SKV_UNUSED(status);

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

        auto results = forEachEntry(std::cbegin(ventries), std::cend(ventries), &IVolume::link, name);
        auto ret = std::any_of(std::cbegin(results), std::cend(results), [](const auto& s) { return s.isOk(); });

        return ret? Status::Ok() : Status::InvalidOperation("Unable to create link");
    }

    [[nodiscard]] Status unlink(Handle handle, std::string_view name) {
        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        auto results = forEachEntry(std::cbegin(ventries), std::cend(ventries), &IVolume::unlink, name);
        auto ret = std::any_of(std::cbegin(results), std::cend(results), [](const auto& s) { return s.isOk(); });

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

        std::sort(std::begin(ventries), std::end(ventries), std::greater<>()); //sort from high priority to low

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
    ThreadPool<> threadPool_;
};

}
