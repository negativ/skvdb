#pragma once

#include "Volume.hpp"

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "ControlBlock.hpp"
#include "Property.hpp"
#include "StorageEngine.hpp"
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

    using storage_type       = StorageEngine<IVolume::Handle,          // key type
                                             std::uint32_t,            // block index type
                                             std::uint32_t,            // bytes count in one record (4GB now)
                                             Volume::Properties,       // type of properties container
                                             Volume::Clock,            // type of used clock (system/steady,etc.)
                                             Volume::InvalidHandle,    // key value of invalid entry
                                             Volume::RootHandle>;      // key value of root entry
    using entry_type         = storage_type::entry_type;
    using cb_type            = ControlBlock<entry_type>;
    using cb_ptr_type        = cb_type::ptr;

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

    [[nodiscard]] Status initialize(std::string_view directory, std::string_view volumeName) {
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

        flushControlBlocks();
        invalidatePathCache();

        return storage_->close();
    }

    [[nodiscard]] bool initialized() const noexcept {
        return storage_->opened();
    }

    [[nodiscard]] std::tuple<Status, Volume::Handle> open(std::string_view p) {
        auto path = simplifyPath(p);

        auto [status, handle, foundPath] = searchCachedPathEntry(path);

        if (status.isOk() && foundPath == path)
            return claimControlBlock(handle);

        if (status.isNotFound()) {
            handle = Volume::RootHandle;
            updatePathCacheEntry("/", handle);
        }
        else
            path.erase(0, foundPath.size()); // removing found part from search path

        auto tokens = split(path, StringPathIterator::separator);
        std::string trackPath = foundPath;

        for (const auto& t : tokens) {
            auto cb = getControlBlock(handle);
            Entry::Children children;

            auto fetchChildren = [&children](auto&& e) {
                children = e.children();
            };

            if (cb) {
                fetchChildren(cb->entry());
            }
            else {
                const auto& [status, handleEntry] = storage_->load(handle);

                if (!status.isOk())
                    return {status, Volume::InvalidHandle};

                fetchChildren(handleEntry);
            }

            auto it = children.find(t);

            if (it == std::cend(children))
                return {NoSuchEntryStatus, Volume::InvalidHandle};

            handle = it->second;

            trackPath += ("/" + t);

            updatePathCacheEntry(trackPath, handle);
        }

        return claimControlBlock(handle);
    }

    [[nodiscard]] Status close(Volume::Handle d)  {
        if (d == Volume::InvalidHandle)
            return NoSuchEntryStatus;

        return releaseControlBlock(d);
    }

    [[nodiscard]] std::tuple<Status, std::set<std::string>> children(Volume::Handle handle) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return {NoSuchEntryStatus, {}};

        std::shared_lock locker(cb->xLock());

        auto& entry = cb->entry();
        auto children = entry.children();

        locker.unlock();

        std::set<std::string> ret;

        std::transform(std::cbegin(children), std::cend(children),
                       std::inserter(ret, std::begin(ret)),
                       [](auto&& p) {
                            const auto& [name, id] = p;
                            static_cast<void>(id);

                            return name;
                       });

        return {Status::Ok(), ret};
    }

    [[nodiscard]] std::tuple<Status, Volume::Properties> properties(Volume::Handle handle) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return {NoSuchEntryStatus, {}};

        std::shared_lock locker(cb->xLock());

        return {Status::Ok(), cb->entry().properties()};
    }

    [[nodiscard]] std::tuple<Status, Volume::PropertiesNames> propertiesNames(Handle handle) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return { NoSuchEntryStatus, {} };

        std::shared_lock locker(cb->xLock());

        return { Status::Ok(), cb->entry().propertiesNames() };
    }

    [[nodiscard]] std::tuple<Status, Property> property(Volume::Handle handle, std::string_view name) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return {NoSuchEntryStatus, {}};

        std::shared_lock locker(cb->xLock());

        return cb->entry().property(util::to_string(name));
    }

    [[nodiscard]] Status setProperty(Volume::Handle handle, std::string_view name, const Property &value) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return NoSuchEntryStatus;

        auto& entry = cb->entry();
        auto propName = util::to_string(name);

        std::unique_lock locker(cb->xLock());

        auto status = entry.setProperty(propName, value);

        if (status.isOk())
            cb->setDirty(true);

        return status;
    }

    [[nodiscard]] Status removeProperty(Volume::Handle handle, std::string_view name) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return NoSuchEntryStatus;

        std::unique_lock locker(cb->xLock());

        auto status = cb->entry().removeProperty(util::to_string(name));

        if (status.isOk())
            cb->setDirty(true);

        return status;
    }

    [[nodiscard]] std::tuple<Status, bool> hasProperty(Volume::Handle handle, std::string_view name) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return {NoSuchEntryStatus, false};

        std::shared_lock locker(cb->xLock());

        return {Status::Ok(), cb->entry().hasProperty(util::to_string(name))};
    }

    [[nodiscard]] Status expireProperty(Volume::Handle handle, std::string_view name, chrono::system_clock::time_point tp) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return NoSuchEntryStatus;

        std::unique_lock locker(cb->xLock());

        auto status = cb->entry().expireProperty(util::to_string(name), chrono::duration_cast<chrono::milliseconds>(tp - chrono::system_clock::now()));

        if (status.isOk())
            cb->setDirty(true);

        return status;
    }

    [[nodiscard]] Status cancelPropertyExpiration(Volume::Handle handle, std::string_view name) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return NoSuchEntryStatus;

        std::unique_lock locker(cb->xLock());

        auto status = cb->entry().cancelPropertyExpiration(util::to_string(name));

        if (status.isOk())
            cb->setDirty(true);

        return status;
    }

    [[nodiscard]] Status createChild(Volume::Handle handle, std::string_view name) {
        auto it = std::find(std::cbegin(name), std::cend(name),
                            StringPathIterator::separator);

        if (it != std::cend(name) || name.empty())
            return Status::InvalidArgument("Invalid name");

        auto cb = getControlBlock(handle);

        if (!cb)
            return NoSuchEntryStatus;

        std::unique_lock locker(cb->xLock());

        auto& entry = cb->entry();
        const auto& children = entry.children();

        auto cit = std::find_if(std::cbegin(children), std::cend(children),
                                [&name](auto&& p) {
                                    const auto& [cname, cid] = p;
                                    static_cast<void>(cid);

                                    return (cname == name);
                                });

        if (cit != std::cend(children))
            return Status::InvalidArgument("Entry already exists");

        entry_type child{storage_->newKey(), util::to_string(name)};

        auto status = entry.addChild(child);

        if (!status.isOk()) {
            storage_->reuseKey(child.handle());

            return status;
        }

        status = storage_->save(child);

        if (!status.isOk()) {
            [[maybe_unused]] auto r = entry.removeChild(child);

            assert(r.isOk());

            return status;
        }

        cb->setDirty(true);

        return Status::Ok();
    }

    [[nodiscard]] Status removeChild(Handle handle, std::string_view name) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return NoSuchEntryStatus;

        std::unique_lock locker(cb->xLock());

        auto& entry = cb->entry();
        const auto& children = entry.children();

        auto it = std::find_if(std::cbegin(children), std::cend(children),
                               [&name](auto&& p) {
                                    const auto& [cname, cid] = p;
                                    static_cast<void>(cid);

                                    return (cname == name);
                               });

        if (it == std::cend(children))
            return NoSuchEntryStatus;

        auto cid = it->second;

        if (getControlBlock(cid))
            return Status::InvalidOperation("Child entry opened");

        {
            const auto& [status, child] = storage_->load(cid);

            if (!status.isOk())
                return status;

            if (!child.children().empty())
                return Status::InvalidArgument("Child entry not empty");
        }

        entry_type child{cid, util::to_string(name)};
        auto status = entry.removeChild(child);

        if (!status.isOk())
            return status;

        cb->setDirty(true);

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

    [[nodiscard]] std::tuple<Status, Volume::Handle> claimControlBlock(Volume::Handle handle) {
        std::unique_lock locker(controlBlocksLock_);

        auto it = controlBlocks_.find(handle);

        if (it != std::end(controlBlocks_)) { // ok, someone already opened this handle
            it->second->claim();              // just increase usage counter

            return {Status::Ok(), handle};
        }

        locker.unlock();

        auto [status, entry] = storage_->load(handle);

        if (!status.isOk())
            return {status, Volume::InvalidHandle};

        return claimControlBlock(handle, std::move(entry));
    }

    [[nodiscard]] std::tuple<Status, Volume::Handle> claimControlBlock(Volume::Handle handle, entry_type&& e) {
        std::unique_lock locker(controlBlocksLock_);

        auto it = controlBlocks_.find(handle);

        if (it != std::end(controlBlocks_)) { // ok, someone already opened this handle
            it->second->claim();              // just increase usage counter of cb

            return {Status::Ok(), handle};
        }

        auto cb = cb_type::create(std::move(e));
        cb->claim();

        controlBlocks_[handle] = cb;

        return {Status::Ok(), handle};
    }

    [[nodiscard]] Status releaseControlBlock(Volume::Handle handle) {
        std::unique_lock locker(controlBlocksLock_);

        auto it = controlBlocks_.find(handle);

        if (it != std::end(controlBlocks_)) {
            auto ptr = it->second;

            ptr->release();

            if (ptr->free()) {    // entry can be synchronyzed with storage
                controlBlocks_.erase(it);

                if (ptr->dirty()) // entry was changed
                    return scheduleControlBlockSync(ptr);
            }

            return Status::Ok();
        }

        return NoSuchEntryStatus;
    }

    [[nodiscard]] cb_ptr_type getControlBlock(Volume::Handle handle) {
        std::shared_lock locker(controlBlocksLock_);

        auto it = controlBlocks_.find(handle);

        if (it != std::end(controlBlocks_))
            return it->second;

        return {};
    }

    [[nodiscard]] Status scheduleControlBlockSync(const cb_ptr_type& cb) {
        if (!cb || !cb->dirty())
            return Status::InvalidArgument("Invalid or clean CB");

        // TODO: implement SYNC queue

        return storage_->save(cb->entry());
    }

    void flushControlBlocks() {
        std::unique_lock locker(controlBlocksLock_);

        for (const auto& [handle, cb] : controlBlocks_) {
            SKV_UNUSED(handle);

            if (cb->dirty())
                SKV_UNUSED(scheduleControlBlockSync(cb));
        }

        controlBlocks_.clear();
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
    std::shared_mutex controlBlocksLock_;
    std::unordered_map<Volume::Handle, cb_ptr_type> controlBlocks_;
    MRUCache<std::string,Volume::Handle, PATH_MRU_CACHE_SIZE> pathCache_;
    mutable SpinLock<> claimLock_;
    Volume::Token claimToken_{};
    std::size_t claimCount_{0};
};

}
