#include "Volume.hpp"

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "ControlBlock.hpp"
#include "Property.hpp"
#include "Storage.hpp"
#include "util/MRUCache.hpp"
#include "util/String.hpp"
#include "util/StringPath.hpp"
#include "util/StringPathIterator.hpp"

namespace skv::ondisk {

struct Volume::Impl {
    static constexpr std::size_t PATH_MRU_CACHE_SIZE = 1024;

    using storage_type       = Storage<Volume::Handle>;
    using entry_type         = storage_type::entry_type;
    using cb_type            = ControlBlock<entry_type>;
    using cb_ptr_type        = cb_type::ptr;

    Impl():
        storage_{new storage_type{}}
    {

    }

    ~Impl() {

    }

    std::tuple<Status, Volume::Handle> open(std::string_view p) {
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
            entry_type::children_type children;

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
                else
                    fetchChildren(handleEntry);
            }

            auto it =std::find_if(std::cbegin(children), std::cend(children),
                                  [&](auto&& p) {
                                      auto&& [name, id] = p;
                                      return (name == t);
                                  });

            if (it == std::cend(children))
                return {Status::InvalidArgument("No such entry"), Volume::InvalidHandle};

            handle = it->second;

            trackPath += ("/" + t);

            updatePathCacheEntry(trackPath, handle);
        }

        return claimControlBlock(handle);
    }

    Status close(Volume::Handle d)  {
        return releaseControlBlock(d);
    }

    std::tuple<Status, std::set<std::string>> children(Volume::Handle handle) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return {Status::InvalidArgument("No such entry"), {}};

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

    std::tuple<Status, std::set<std::string> > properties(Volume::Handle handle) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return {Status::InvalidArgument("No such entry"), {}};

        std::shared_lock locker(cb->xLock());

        return {Status::Ok(), cb->entry().propertiesSet()};
    }

    std::tuple<Status, Property> property(Volume::Handle handle, std::string_view name) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return {Status::InvalidArgument("No such entry"), {}};

        std::shared_lock locker(cb->xLock());

        return cb->entry().property(util::to_string(name));
    }

    Status setProperty(Volume::Handle handle, std::string_view name, const Property &value) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return Status::InvalidArgument("No such entry");

        std::unique_lock locker(cb->xLock());

        auto status = cb->entry().setProperty(util::to_string(name), value);

        if (status.isOk())
            cb->setDirty(true);

        return status;
    }

    Status removeProperty(Volume::Handle handle, std::string_view name) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return Status::InvalidArgument("No such entry");

        std::unique_lock locker(cb->xLock());

        auto status = cb->entry().removeProperty(util::to_string(name));

        if (status.isOk())
            cb->setDirty(true);

        return status;
    }

    std::tuple<Status, bool> hasProperty(Volume::Handle handle, std::string_view name) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return {Status::InvalidArgument("No such entry"), false};

        std::shared_lock locker(cb->xLock());

        return {Status::Ok(), cb->entry().hasProperty(util::to_string(name))};
    }

    Status expireProperty(Volume::Handle handle, std::string_view name, chrono::system_clock::time_point tp) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return Status::InvalidArgument("No such entry");

        std::unique_lock locker(cb->xLock());

        auto status = cb->entry().expireProperty(util::to_string(name), tp);

        if (status.isOk())
            cb->setDirty(true);

        return status;
    }

    Status cancelPropertyExpiration(Volume::Handle handle, std::string_view name) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return Status::InvalidArgument("No such entry");

        std::unique_lock locker(cb->xLock());

        auto status = cb->entry().cancelPropertyExpiration(util::to_string(name));

        if (status.isOk())
            cb->setDirty(true);

        return status;
    }

    Status createChild(Volume::Handle handle, std::string_view name) {
        auto it = std::find(std::cbegin(name), std::cend(name),
                            StringPathIterator::separator);

        if (it != std::cend(name) || name.empty())
            return Status::InvalidArgument("Invalid name (empty or contains restricted chracters)");

        auto cb = getControlBlock(handle);

        if (!cb)
            return Status::InvalidArgument("No such entry");

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
            storage_->reuseKey(child.key());

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

    Status removeChild(Handle handle, std::string_view name) {
        auto cb = getControlBlock(handle);

        if (!cb)
            return Status::InvalidArgument("No such entry");

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
            return Status::InvalidArgument("No such entry");

        auto cid = it->second;

        {
            auto ccb = getControlBlock(cid);

            if (!ccb)
                return Status::InvalidOperation("Child entry shouldn't be opened");
        }
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

    std::tuple<Status, Volume::Handle, std::string> searchCachedPathEntry(const std::string& path) {
        ReverseStringPathIterator start{path}, stop{};

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

    bool invalidatePathCacheEntry(const std::string& path) {
        return pathCache_.remove(path);
    }

    std::tuple<Status, Volume::Handle> claimControlBlock(Volume::Handle handle) {
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

    std::tuple<Status, Volume::Handle> claimControlBlock(Volume::Handle handle, entry_type&& e) {
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

    Status releaseControlBlock(Volume::Handle handle) {
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

        return Status::InvalidArgument("No such entry");
    }

    cb_ptr_type getControlBlock(Volume::Handle handle) {
        std::shared_lock locker(controlBlocksLock_);

        auto it = controlBlocks_.find(handle);

        if (it != std::end(controlBlocks_))
            return it->second;

        return {};
    }

    Status scheduleControlBlockSync(cb_ptr_type cb) {
        if (!cb || !cb->dirty())
            return Status::InvalidArgument("Invalid or clean CB");

        // TODO: implement SYNC queue

        return storage_->save(cb->entry());
    }

    std::unique_ptr<storage_type> storage_;
    std::shared_mutex controlBlocksLock_;
    std::unordered_map<Volume::Handle, cb_ptr_type> controlBlocks_;
    std::list<cb_ptr_type> syncQueue_;
    std::mutex syncQueueLock_;
    MRUCache<std::string,Volume::Handle, PATH_MRU_CACHE_SIZE> pathCache_;
};

Volume::Volume():
    impl_{new Impl{}}
{

}

Volume::~Volume() noexcept {

}

Status Volume::initialize(std::string_view directory, std::string_view volumeName) {
    if (initialized())
        return Status::InvalidOperation("Volume already opened");

    auto status = impl_->storage_->open(directory, volumeName);

    if (status.isOk()) {
         auto [status, handle] = impl_->open("/"); // implicitly open root item

         assert(handle == RootHandle);

         return status;
    }

    return status;
}

Status Volume::deinitialize() {
    if (!initialized())
        return Status::InvalidOperation("Volume not opened");

    // TODO: sync storage
    static_cast<void>(impl_->close(RootHandle));

    return impl_->storage_->close();
}

bool Volume::initialized() const noexcept {
    return impl_->storage_->opened();
}

std::tuple<Status, Volume::Handle> Volume::open(std::string_view path) {
    if (!initialized())
        return {Status::InvalidOperation("Volume not opened"), Volume::InvalidHandle};

    return impl_->open(path);
}

Status Volume::close(Volume::Handle d) {
    if (!initialized())
        return Status::InvalidOperation("Volume not opened");

    return impl_->close(d);
}

std::tuple<Status, std::set<std::string> > Volume::children(Volume::Handle h) {
    if (!initialized())
        return {Status::InvalidOperation("Volume not opened"), {}};

    return impl_->children(h);
}

std::tuple<Status, std::set<std::string> > Volume::properties(Volume::Handle h) {
    if (!initialized())
        return {Status::InvalidOperation("Volume not opened"), {}};

    return impl_->properties(h);
}

std::tuple<Status, Property> Volume::property(Volume::Handle h, std::string_view name) {
    if (!initialized())
        return {Status::InvalidOperation("Volume not opened"), {}};

    return impl_->property(h, name);
}

Status Volume::setProperty(Volume::Handle h, std::string_view name, const Property &value) {
    if (!initialized())
        return Status::InvalidOperation("Volume not opened");

    return impl_->setProperty(h, name, value);
}

Status Volume::removeProperty(Volume::Handle h, std::string_view name) {
    if (!initialized())
        return Status::InvalidOperation("Volume not opened");

    return impl_->removeProperty(h, name);
}

std::tuple<Status, bool> Volume::hasProperty(Volume::Handle h, std::string_view name) {
    if (!initialized())
        return {Status::InvalidOperation("Volume not opened"), false};

    return impl_->hasProperty(h, name);
}

Status Volume::expireProperty(Volume::Handle h, std::string_view name, chrono::system_clock::time_point tp) {
    if (!initialized())
        return Status::InvalidOperation("Volume not opened");

    return impl_->expireProperty(h, name, tp);
}

Status Volume::cancelPropertyExpiration(Volume::Handle h, std::string_view name) {
    if (!initialized())
        return Status::InvalidOperation("Volume not opened");

    return impl_->cancelPropertyExpiration(h, name);
}

Status Volume::link(Volume::Handle h, std::string_view name) {
    if (!initialized())
        return Status::InvalidOperation("Volume not opened");

    return impl_->createChild(h, name);
}

Status Volume::unlink(Handle h, std::string_view name) {
    if (!initialized())
        return Status::InvalidOperation("Volume not opened");

    return impl_->removeChild(h, name);
}


}
