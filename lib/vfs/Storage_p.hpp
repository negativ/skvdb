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

#include "IVolume.hpp"
#include "Storage.hpp"
#include "MountPoint.hpp"
#include "vfs/VirtualEntry.hpp"
#include "util/Log.hpp"
#include "util/SpinLock.hpp"
#include "util/Status.hpp"
#include "util/String.hpp"
#include "util/StringPath.hpp"
#include "util/StringPathIterator.hpp"
#include "util/ThreadPool.hpp"
#include "util/Unused.hpp"

namespace skv::vfs {

using namespace skv::util;

struct Storage::Impl {
    static constexpr Status InvalidVolumeArgumentStatus = Status::InvalidArgument("Invalid volume");
    const skv::util::Status InvalidTokenStatus  = skv::util::Status::InvalidArgument("Invalid token");

	static constexpr const char* const TAG = "vfs::Storage";

    enum class ChildOp {
        Link,
        Unlink
    };

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

    [[nodiscard]] Status childOperation(IEntry &entry, const std::string& name, ChildOp op) {
        using result      = Status;
        using future      = std::future<result>;
        using future_list = std::vector<future>;
        using result_list = std::vector<result>;

        std::shared_lock locker{openedEntriesLock_};

        auto it = openedEntries_.find(entry.handle());

        if (it == std::end(openedEntries_))
                return Status::InvalidArgument("No such entry");

        auto* entryPtr = it->second;
        assert(entryPtr != nullptr);

        if (&entry != static_cast<IEntry*>(entryPtr))
            return Status::InvalidArgument("No such entry");

        future_list futures;

        try {
            auto& volumes = entryPtr->volumes();
            auto& entries = entryPtr->entries();

            assert(volumes.size() == entries.size());

            for (std::size_t i = 0; i < volumes.size(); ++i) {
                auto& volume = volumes[i];
                auto& entry = entries[i];

                switch (op) {
                case ChildOp::Link:
                    futures.emplace_back(threadPool_.schedule([&]() -> result { return volume->link(*entry, name); }));
                    break;
                case ChildOp::Unlink:
                    futures.emplace_back(threadPool_.schedule([&]() -> result { return volume->unlink(*entry, name); }));
                    break;
                }
            }
        }
        catch (const std::exception &e) {
            Log::e(TAG, e.what());

            return Status::Fatal("Unknown error");
        }

        waitAllFutures(std::begin(futures), std::end(futures));

        result_list results;

        for (auto& f : futures) {
            try {
                results.emplace_back(f.get());
            }
            catch (const std::bad_alloc&) {} // just ignore bad_alloc
            catch (const std::exception& e) { Log::e(TAG, "Storage::childOperation() exception: ", e.what()); }
            catch (...) { Log::e(TAG, "Storage::childOperation() unknown exception"); }
        }

        auto ok = std::any_of(std::cbegin(results), std::cend(results),
                              [](auto&& status) { return status.isOk(); });

        return ok? Status::Ok() : Status::InvalidArgument("Unknown error");
    }

    std::shared_ptr<IEntry> entry(const std::string& path) {
        using result      = std::shared_ptr<IEntry>;
        using future      = std::future<std::pair<std::shared_ptr<IVolume>, result>>;
        using future_list = std::vector<future>;
        using result_list = std::vector<result>;

        std::string vpath = simplifyPath(path);
        auto [status, mountPath, mountEntries] = searchMountPathFor(vpath);

        if (!status.isOk())
            return {};

		std::sort(std::begin(mountEntries), std::end(mountEntries), std::greater<>{}); //sort from high priority to low

        auto subvpath = vpath.substr(mountPath.size(), vpath.size() - mountPath.size()); // extracting subpath from vpath
        future_list futures;

        try {
			for (auto& mentry : mountEntries) {
                futures.emplace_back(threadPool_.schedule([&]() -> std::pair<std::shared_ptr<IVolume>, result> {
															auto volume = mentry.volume();
                                                            auto subpath = simplifyPath(mentry.entryPath() + "/" + subvpath);
                                                            return {volume, volume->entry(subpath)};
														 }));
			}
        }
        catch (const std::bad_alloc&) { return {};  }// it's not to safe to call any function
        catch (const std::exception &e) {
			Log::e(TAG, e.what());

            return {};
        }

        waitAllFutures(std::begin(futures), std::end(futures));

        result_list results;
        VirtualEntry::Volumes volumes;

		for (auto& f : futures) {
			try {
                auto [volume, entry] = f.get();

                volumes.emplace_back(std::move(volume));
                results.emplace_back(std::move(entry));
			}
            catch (const std::bad_alloc&) {} // just ignore bad_alloc
            catch (const std::exception& e) { Log::e(TAG, "Storage::entry() exception: ", e.what()); }
            catch (...) { Log::e(TAG, "Storage::entry(): unknown exception"); }
		}

        if (results.empty() || (volumes.size() != results.size()))
			return {};

        auto ptr = std::shared_ptr<VirtualEntry>(new VirtualEntry{newHandle(), std::move(results), std::move(volumes), threadPool_},
                                                 [this](VirtualEntry* ptr) {
                                                     unregisterEntry(ptr);

                                                     delete ptr;
                                                 });

        return registerEntry(ptr.get())? std::static_pointer_cast<vfs::IEntry>(ptr) : std::shared_ptr<IEntry>{nullptr};
    }

    Status link(IEntry &entry, const std::string& name) {
        return childOperation(entry, name, ChildOp::Link);
    }

    Status unlink(IEntry &entry, const std::string& name) {
        return childOperation(entry, name, ChildOp::Unlink);
    }

    Status claim(IVolume::Token token) noexcept {
        std::unique_lock locker(claimLock_);

        if (claimToken_ != IVolume::Token{} &&
            claimToken_ != token)
        {
            return InvalidTokenStatus;
        }

        if (token == IVolume::Token{})
            return InvalidTokenStatus;

        claimToken_ = token;
        ++claimCount_;

        return Status::Ok();
    }

    Status release(IVolume::Token token) noexcept {
        std::unique_lock locker(claimLock_);

        if (claimToken_ == IVolume::Token{})
            return Status::InvalidOperation("Volume not claimed");

        if (claimToken_ != token)
            return InvalidTokenStatus;

        --claimCount_;

        if (claimCount_ == 0)
            claimToken_ = IVolume::Token{};

        return Status::Ok();
    }

    Status mount(const std::shared_ptr<IVolume>& volume, const std::string& entryPath, const std::string& mountPath, Storage::Priority prio) {
        if (!volume)
            return InvalidVolumeArgumentStatus;

        if (!volume->claim(this).isOk())
            return Status::InvalidOperation("Volume already claimed");

        mount::Entry entry(mountPath, entryPath, volume, prio);

        if (!entry.open()) {
            SKV_UNUSED(volume->release(this));

            return Status::InvalidArgument("Unable to mount");
        }

        std::unique_lock locker(mpointsLock_);

        auto& index = mpoints_.get<mount::tags::ByAll>();
        auto retp = index.insert(entry);

        if (!retp.second) {
            SKV_UNUSED(volume->release(this));
            entry.close();

            return Status::InvalidArgument("Already mounted");
        }

        return Status::Ok();
    }

    Status unmount(std::shared_ptr<IVolume> volume, const std::string& entryPath, const std::string& mountPath) {
        if (!volume)
            return InvalidVolumeArgumentStatus;

        std::unique_lock locker(mpointsLock_);

        auto& index = mpoints_.get<mount::tags::ByMountPath>();
        auto [start, stop] = index.equal_range(mountPath);
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

    [[nodiscard]] bool registerEntry(VirtualEntry* entry) {
        if (!entry)
            return false;

        std::unique_lock locker{openedEntriesLock_};

        return openedEntries_.insert(std::make_pair(entry->handle(), entry)).second;
    }

    bool unregisterEntry(VirtualEntry* entry) {
        if (!entry)
            return false;

        std::unique_lock locker{openedEntriesLock_};

        return openedEntries_.erase(entry->handle()) > 0;
    }

    [[nodiscard]] std::tuple<Status, std::string, std::vector<mount::Entry>> searchMountPathFor(const std::string& path) const {
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

private:
    mount::Points mpoints_{};
    std::atomic<Storage::Handle> currentHandle_{IVolume::RootHandle + 1};
    mutable std::shared_mutex mpointsLock_;
    std::unordered_map<IEntry::Handle, VirtualEntry*> openedEntries_; // alternative - using dynamic_cast
    std::shared_mutex openedEntriesLock_;
    ThreadPool threadPool_;
    mutable SpinLock<> claimLock_;
    IVolume::Token claimToken_{};
    std::size_t claimCount_{0};
};

}
