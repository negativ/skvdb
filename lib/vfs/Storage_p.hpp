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
    static constexpr Status InvalidVolumeArgumentStatus = Status::InvalidArgument("Invalid volume");

//    template <typename Iterator, typename F, typename ... Args>
//    auto forEachEntry(Iterator start, Iterator stop, F&& func, Args&& ... args) {
//        using namespace std::literals;
//        using result      = std::invoke_result_t<F, IVolume*, IVolume::Handle, Args...>;
//        using future      = std::future<result>;
//        using future_list = std::vector<future>;
//        using result_list = std::vector<result>;

//        result_list results;
//        future_list futures;

//        auto dist = std::distance(start, stop);

//        if (dist > 1) {
//            auto it = start;

//            std::advance(start, 1);// we will do the first call in current thread context

//            try {
//                std::for_each(start, stop,
//                              [&](const auto& ventry) {
//                                  if (auto volume = ventry.volume().lock(); volume)
//                                      futures.emplace_back(threadPool_.schedule(std::forward<F>(func), volume.get(), ventry.handle(), std::forward<Args>(args)...));
//                              });
//            }
//            catch (const std::exception& e) {
//                Log::e("vfs::Storage", "Exception: ", e.what());
//            }

//            if (auto volume = it->volume().lock(); volume)
//                results.emplace_back(std::invoke(std::forward<F>(func), volume.get(), it->handle(), std::forward<Args>(args)...));

//            waitAllFutures(std::begin(futures), std::end(futures));

//            std::transform(std::begin(futures), std::end(futures),
//                           std::back_inserter(results),
//                           [](auto& f) { return f.get(); });
//        }
//        else if (dist == 1) {
//            if (auto volume = start->volume().lock(); volume)
//                results.emplace_back(std::invoke(std::forward<F>(func), volume.get(), start->handle(), std::forward<Args>(args)...));
//        }

//        return  results;
//    }

//    template <typename Iterator>
//    void waitAllFutures(Iterator start, Iterator stop) {
//        using namespace std::literals;

//        while (!std::all_of(start, stop,
//                            [](auto& f) { return (f.wait_for(0ms) == std::future_status::ready); }))
//            threadPool_.throttle(); // helping thread pool to do his work
//    }

    Impl() = default;
    ~Impl() noexcept = default;

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    Impl(Impl&&) noexcept = delete;
    Impl& operator=(Impl&&) noexcept = delete;

//    [[nodiscard]] std::tuple<Status, Storage::Handle> open(std::string_view path) {
//        using result      = std::tuple<Status, VirtualEntry>;
//        using future      = std::future<result>;
//        using future_list = std::vector<future>;
//        using result_list = std::vector<result>;

//        std::string vpath = simplifyPath(path);
//        auto [status, mountPath, mountEntries] = searchMountPathFor(vpath);

//        if (!status.isOk())
//            return {status, Storage::InvalidHandle};

//        auto subvpath = vpath.substr(mountPath.size(), vpath.size() - mountPath.size()); // extracting subpath from vpath
//        future_list futures;

//        try {
//            std::transform(std::begin(mountEntries), std::end(mountEntries),
//                           std::back_inserter(futures),
//                           [&](auto&& entry) {
//                               return threadPool_.schedule([&]() -> std::tuple<Status, VirtualEntry> {
//                                   auto volume  = entry.volume();
//                                   auto subpath = simplifyPath(entry.entryPath() + "/" + subvpath);
//                                   auto [status, handle] = volume->open(subpath);

//                                   if (status.isOk())
//                                       return {status, VirtualEntry(subpath, volume, handle, entry.priority())};

//                                   return {Status::InvalidArgument("Invalid volume"), VirtualEntry{}};
//                               });
//                           });
//        }
//        catch (const std::exception &e) {
//            return {Status::Fatal("Exception"), Storage::InvalidHandle};
//        }

//        waitAllFutures(std::begin(futures), std::end(futures));

//        result_list results;

//        std::transform(std::begin(futures), std::end(futures),
//                       std::back_inserter(results),
//                       [](auto& f) { return f.get(); });

//        VirtualEntries openedEntries;

//        for (const auto& [status, entry] : results) {
//            if (status.isOk())
//                openedEntries.emplace_back(entry);
//        }

//        if (openedEntries.empty())
//            return {Status::InvalidArgument("No such path"), Storage::InvalidHandle};

//        return {Status::Ok(), addVirtualEntries(std::move(openedEntries))};
//    }

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
            return Status::InvalidOperation("Volume already claimed");

        mount::Entry entry(mountPath, entryPath, volume, prio);

        if (!entry.open()) {
            SKV_UNUSED(volume->release(this));

            return Status::InvalidArgument("Unable to create mount point");
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

    [[nodiscard]] Storage::Handle newHandle() noexcept {
        return currentHandle_.fetch_add(1);
    }

    mount::Points mpoints_{};
    std::atomic<Storage::Handle> currentHandle_{IVolume::RootHandle + 1};
    mutable std::shared_mutex mpointsLock_;
    ThreadPool<> threadPool_;
};

}
