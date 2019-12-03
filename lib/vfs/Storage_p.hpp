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
#include "util/Log.hpp"
#include "util/Status.hpp"
#include "util/String.hpp"
#include "util/StringPath.hpp"
#include "util/StringPathIterator.hpp"
#include "util/ThreadPool.hpp"
#include "util/Unused.hpp"

namespace skv::vfs {

using namespace skv::util;

using ThreadPool = util::ThreadPool<>;

class EntryImpl final : public vfs::IEntry {
public:
	using Entries = std::vector<std::shared_ptr<IEntry>>;
	using Volumes = std::vector<IVolumePtr>;

	EntryImpl(Handle handle, Entries&& entries, Volumes&& volumes, ThreadPool& threadPool):
		handle_{handle},
		entries_{entries},
		volumes_{volumes},
		threadPool_{std::ref(threadPool)}
	{

	}

	~EntryImpl() noexcept override = default;

	Handle handle() const noexcept override {
		return handle_;
	}

	std::string name() const override {
		return ""; // TODO: implement
	}

	bool hasProperty(const std::string& prop) const noexcept override {
		return false; // TODO: implement
	}

	Status setProperty(const std::string& prop, const Property& value) override {
		return Status::IOError(""); // TODO: implement
	}

	std::tuple<Status, Property> property(const std::string& prop) const override {
		return { Status::IOError(""), {} }; // TODO: implement
	}

	Status removeProperty(const std::string& prop) override {
		return Status::IOError(""); // TODO: implement
	}

	Properties properties() const override {
		return {}; // TODO: implement
	}

	std::set<std::string> propertiesNames() const override {
		return {}; // TODO: implement
	}

	Status expireProperty(const std::string& prop, chrono::milliseconds ms) override {
		return Status::IOError(""); // TODO: implement
	}

	Status cancelPropertyExpiration(const std::string& prop) override {
		return Status::IOError(""); // TODO: implement
	}

	std::set<std::string> children() const override {
		return {}; // TODO: implement
	}

private:
	template <typename Iterator>
	void waitAllFutures(Iterator start, Iterator stop) {
		using namespace std::literals;

		while (!std::all_of(start, stop,
			[](auto& f) { return (f.wait_for(0ms) == std::future_status::ready); }))
			threadPool_.throttle(); // helping thread pool to do his work
	}

	template <typename Iterator, typename F, typename ... Args>
    auto forEachEntry(F&& func, Args&& ... args) {
        using namespace std::literals;
        using result      = std::invoke_result_t<F, IEntry*, Args...>;
        using future      = std::future<result>;
        using future_list = std::vector<future>;
        using result_list = std::vector<result>;

		auto start = std::begin(entries_),
			 stop  = std::end(entries_);
        
		if (start == stop)
			return result_list{};

		result_list results;
		future_list futures;

		auto it = start;

		std::advance(start, 1); // first call in current thread context

		while (start != stop) {
			auto& entry = (*start);

			try {
				futures.emplace_back(threadPool_.schedule(std::forward<F>(func), entry.get(), std::forward<Args>(args)...));
			}
			catch (...) { Log::e(TAG, "Unknown exception. Ignoring"); } 

			++start;
		}

		try {
			results.emplace_back(std::invoke(std::forward<F>(func), (*it).get(), std::forward<Args>(args)...));
		}
		catch (...) { Log::e(TAG, "Unknown exception. Ignoring"); }

		waitAllFutures(std::begin(futures), std::end(futures));

		for (auto& f : futures) {
			try {
				results.emplace_back(f.get());
			}
			catch (...) { Log::e(TAG, "Unknown exception. Ignoring"); }
		}

        return  results;
    }

	Handle handle_;
	Entries entries_;
	Volumes volumes_;
	std::reference_wrapper<ThreadPool> threadPool_;
};

struct Storage::Impl {
    static constexpr Status InvalidVolumeArgumentStatus = Status::InvalidArgument("Invalid volume");
	static constexpr const char* const TAG = "vfs::Storage";

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

    [[nodiscard]] std::shared_ptr<IEntry> entry(std::string_view path) {
        using result      = std::shared_ptr<IEntry>;
        using future      = std::future<result>;
        using future_list = std::vector<future>;
        using result_list = std::vector<result>;

        std::string vpath = simplifyPath(path);
        auto [status, mountPath, mountEntries] = searchMountPathFor(vpath);

        if (!status.isOk())
            return {};

		std::sort(std::begin(mountEntries), std::end(mountEntries), std::greater<>{}); //sort from high priority to low

        auto subvpath = vpath.substr(mountPath.size(), vpath.size() - mountPath.size()); // extracting subpath from vpath
        future_list futures;
		EntryImpl::Volumes volumes;

        try {
			for (auto& mentry : mountEntries) {
				futures.emplace_back(threadPool_.schedule([&]() -> result {
															auto volume = mentry.volume();
															auto subpath = simplifyPath(mentry.entryPath() + "/" + subvpath);
															return volume->entry(subpath);
														 }));
				volumes.emplace_back(mentry.volume());
			}
        }
        catch (const std::exception &e) {
			Log::e(TAG, e.what());

            return {};
        }

        waitAllFutures(std::begin(futures), std::end(futures));

        result_list results;

		for (auto& f : futures) {
			try {
				results.emplace_back(f.get());
			}
			catch (...) { Log::e(TAG, "Unknown exception. Ignoring"); }
		}

		if (results.empty())
			return {};

		auto ptr = std::make_shared<EntryImpl>(newHandle(), std::move(results), std::move(volumes), threadPool_);

		return std::static_pointer_cast<vfs::IEntry>(ptr);
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
    ThreadPool threadPool_;
};

}
