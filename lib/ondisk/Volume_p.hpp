#pragma once

#include "Volume.hpp"

#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "Entry.hpp"
#include "Property.hpp"
#include "StorageEngine.hpp"
#include "vfs/IEntry.hpp"
#include "util/Log.hpp"
#include "util/MRUCache.hpp"
#include "util/SpinLock.hpp"
#include "util/String.hpp"
#include "util/StringPath.hpp"
#include "util/StringPathIterator.hpp"

namespace skv::ondisk {

struct Volume::Impl {
    const skv::util::Status NoSuchEntryStatus   = skv::util::Status::InvalidArgument("No such entry");
    const skv::util::Status InvalidTokenStatus  = skv::util::Status::InvalidArgument("Invalid token");

    static constexpr std::size_t PATH_MRU_CACHE_SIZE = 1024;

    using EntryPtr  = std::shared_ptr<Entry>;
    using EntryWPtr = std::weak_ptr<Entry>;
    using storage_type       = StorageEngine<IVolume::Handle,          // key type
                                             std::uint32_t,            // block index type
                                             std::uint32_t,            // bytes count in one record (4GB now)
                                             IEntry::Properties,       // type of properties container
                                             std::chrono::system_clock,// type of used clock (system/steady,etc.)
                                             Volume::InvalidHandle,    // key value of invalid entry
                                             Volume::RootHandle>;      // key value of root entry

    Impl(Volume::OpenOptions opts):
        storage_{std::make_unique<storage_type>()},
        opts_{opts}
    {

    }

    ~Impl() noexcept = default;

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    Impl(Impl&&) noexcept = delete;
    Impl& operator=(Impl&&) noexcept = delete;

    [[nodiscard]] Status initialize(const fs::path& directory, const std::string& volumeName) {
        storage_type::OpenOptions storageOpts;

        storageOpts.CompactionRatio = opts_.CompactionRatio;
        storageOpts.CompactionDeviceMinSize = opts_.CompactionDeviceMinSize;
        storageOpts.LogDeviceBlockSize = opts_.LogDeviceBlockSize;
        storageOpts.LogDeviceCreateNewIfNotExist = opts_.LogDeviceCreateNewIfNotExist;

        return storage_->open(directory, volumeName, storageOpts);
    }

    [[nodiscard]] Status deinitialize() {
        if (claimed())
            return Status::InvalidOperation("Storage claimed");

        flushEntries();
        invalidatePathCache();

        return storage_->close();
    }

    [[nodiscard]] bool initialized() const noexcept {
        return storage_->opened();
    }

    std::shared_ptr<IEntry> entry(const std::string& p) {
        auto path = simplifyPath(p);

        auto [status, handle, foundPath] = searchCachedPathEntry(path);

        if (status.isOk() && foundPath == path)
            return std::static_pointer_cast<vfs::IEntry>(createEntryForHandle(handle));

        if (status.isNotFound()) {
            handle = Volume::RootHandle;
            updatePathCacheEntry("/", handle);
        }
        else
            path.erase(0, foundPath.size()); // removing found part from search path

        auto tokens = split(path, StringPathIterator::separator);
        std::string trackPath = foundPath;

        for (const auto& t : tokens) {
            auto cb = getEntry(handle);
            Record::Children children;

            auto fetchChildren = [&children](auto&& e) {
                children = e.children();
            };

            if (cb) {
                fetchChildren(cb->record());
            }
            else {
                const auto& [status, handleEntry] = storage_->load(handle);

                if (!status.isOk())
                    return {};

                fetchChildren(handleEntry);
            }

            auto it = children.find(t);

            if (it == std::cend(children))
                return {};

            handle = it->second;

            trackPath += ("/" + t);

            updatePathCacheEntry(trackPath, handle);
        }

        return std::static_pointer_cast<vfs::IEntry>(createEntryForHandle(handle));
    }

    [[nodiscard]] Status createChild(IEntry& e, std::string_view name) {
        auto it = std::find(std::cbegin(name), std::cend(name),
                            StringPathIterator::separator);

        if (it != std::cend(name) || name.empty())
            return Status::InvalidArgument("Invalid name");

        auto entry = getEntry(e.handle());

        if (!entry)
            return NoSuchEntryStatus;

        if (std::addressof(e) != static_cast<IEntry*>(entry.get()))
            return Status::InvalidArgument("Invalid entry");

        std::unique_lock locker(entry->xLock());

        auto& record = entry->record();
        const auto& children = record.children();

        auto cit = std::find_if(std::cbegin(children), std::cend(children),
                                [&name](auto&& p) {
                                    const auto& [cname, cid] = p;
                                    SKV_UNUSED(cid);

                                    return (cname == name);
                                });

        if (cit != std::cend(children))
            return Status::InvalidArgument("Entry already exists");

        Record child{storage_->newKey(), util::to_string(name)};

        auto status = record.addChild(child);

        if (!status.isOk()) {
            storage_->reuseKey(child.handle());

            return status;
        }

        status = storage_->save(child);

        if (!status.isOk()) {
            [[maybe_unused]] auto r = record.removeChild(child);

            assert(r.isOk());

            return status;
        }

        entry->setDirty(true);

        return Status::Ok();
    }

    [[nodiscard]] Status removeChild(IEntry& e, std::string_view name) {
        auto entry = getEntry(e.handle());

        if (!entry)
            return NoSuchEntryStatus;

        if (std::addressof(e) != static_cast<IEntry*>(entry.get()))
            return Status::InvalidArgument("Invalid entry");

        std::unique_lock locker(entry->xLock());

        auto& record = entry->record();
        const auto& children = record.children();

        auto it = std::find_if(std::cbegin(children), std::cend(children),
                               [&name](auto&& p) {
                                    const auto& [cname, cid] = p;
                                    static_cast<void>(cid);

                                    return (cname == name);
                               });

        if (it == std::cend(children))
            return NoSuchEntryStatus;

        auto cid = it->second;

        if (getEntry(cid))
            return Status::InvalidOperation("Child entry opened");

        {
            const auto& [status, child] = storage_->load(cid);

            if (!status.isOk())
                return status;

            if (!child.children().empty())
                return Status::InvalidArgument("Child entry not empty");
        }

        Record child{cid, util::to_string(name)};
        auto status = record.removeChild(child);

        if (!status.isOk())
            return status;

        entry->setDirty(true);

        return storage_->remove(child);
    }

    [[nodiscard]] std::tuple<Status, Volume::Handle, std::string> searchCachedPathEntry(const std::string& path) {
        ReverseStringPathIterator start{path},
                                  stop{};

        Volume::Handle handle;

        while (start != stop) {
            if (pathCache_.lookup(*start, handle)) // cache hit
                return {Status::Ok(), handle, *start};

            ++start;
        }

        return {Status::NotFound("Cache miss"), {}, {}};
    }

    void updatePathCacheEntry(const std::string& path, Volume::Handle h) {
        pathCache_.insert(path, h);
    }

    [[nodiscard]] bool invalidatePathCacheEntry(const std::string& path) {
        return pathCache_.remove(path);
    }

    void invalidatePathCache() {
        pathCache_.clear();
    }

    [[nodiscard]] EntryPtr createEntryForHandle(Volume::Handle handle) {
        std::unique_lock locker{openedEntriesLock_};

        auto it = openedEntries_.find(handle);

        if (it != std::end(openedEntries_)) // ok, someone already opened this handle
            return it->second.lock();

        locker.unlock();

        auto [status, entry] = storage_->load(handle);

        if (!status.isOk())
            return {};

        return createEntryForHandle(handle, std::move(entry));
    }

    [[nodiscard]] EntryPtr createEntryForHandle(Volume::Handle handle, Record&& record) {
        std::unique_lock locker{openedEntriesLock_};

        auto it = openedEntries_.find(handle);

        if (it != std::end(openedEntries_)) // ok, someone already opened this handle
            return it->second.lock();

        auto ptr = std::make_unique<Entry>(std::move(record));
        auto deleter = [this](Entry *e) { releaseEntry(e); };
        auto entry = std::shared_ptr<Entry>{ptr.release(), deleter};

        openedEntries_[handle] = EntryWPtr{entry};

        return entry;
    }

    void releaseEntry(Entry* entry) {
        if (!entry)
            return;

        {
            std::unique_lock locker{openedEntriesLock_};

            openedEntries_.erase(entry->record().handle());
        }

        if (entry->dirty()) {
            auto r = syncRecord(entry->record());
            SKV_UNUSED(r);
        }

        delete entry;
    }

    [[nodiscard]] EntryPtr getEntry(Volume::Handle handle) {
        std::shared_lock locker{openedEntriesLock_};

        auto it = openedEntries_.find(handle);

        if (it != std::end(openedEntries_))
            return it->second.lock();

        return {};
    }

    [[nodiscard]] Status syncRecord(Record& r) {
        return storage_->save(r);
    }

    void flushEntries() {
        std::unique_lock locker{openedEntriesLock_};

        for (const auto& [handle, ewptr] : openedEntries_) {
            SKV_UNUSED(handle);

            if (auto e = ewptr.lock(); e && e->dirty())
                SKV_UNUSED(syncRecord(e->record()));
        }

        openedEntries_.clear();
    }

    [[nodiscard]] Status claim(Volume::Token token) noexcept {
        std::unique_lock locker(claimLock_);

        if (claimToken_ != Volume::Token{} &&
            claimToken_ != token)
        {
            return InvalidTokenStatus;
        }

        if (token == Volume::Token{})
            return InvalidTokenStatus;

        claimToken_ = token;
        ++claimCount_;

        return Status::Ok();
    }

    [[nodiscard]] Status release(Volume::Token token) noexcept {
        std::unique_lock locker(claimLock_);

        if (claimToken_ == Volume::Token{})
            return Status::InvalidOperation("Volume not claimed");

        if (claimToken_ != token)
            return InvalidTokenStatus;

        --claimCount_;

        if (claimCount_ == 0)
            claimToken_ = Volume::Token{};

        return Status::Ok();
    }

    [[nodiscard]] bool claimed() const noexcept {
        std::unique_lock locker(claimLock_);

        return claimCount_ != 0;
    }

    std::unique_ptr<storage_type> storage_;
    Volume::OpenOptions opts_;
    std::shared_mutex openedEntriesLock_;
    std::unordered_map<Volume::Handle, EntryWPtr> openedEntries_;
    MRUCache<std::string, Volume::Handle, PATH_MRU_CACHE_SIZE> pathCache_;
    mutable SpinLock<> claimLock_;
    Volume::Token claimToken_{};
    std::size_t claimCount_{0};
};

}
