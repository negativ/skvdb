#include "Volume.hpp"

#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "Property.hpp"
#include "Storage.hpp"
#include "util/MRUCache.hpp"
#include "util/String.hpp"
#include "util/StringPath.hpp"
#include "util/StringPathIterator.hpp"

namespace skv::ondisk {

struct Volume::Impl {
    using storage_type = Storage<Volume::Handle>;

    static constexpr std::size_t MAX_THREADS = 47;

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
            return {status, handle};

        if (status.isNotFound())
            handle = storage_type::RootEntryId;

        path.erase(0, foundPath.size());

        auto tokens = split(path, '/');
        std::string reconstructedPath = foundPath;

        reconstructedPath.reserve(path.size());

        for (const auto& t : tokens) {
            auto [status, entry] = storage_->load(handle);
                if (!status.isOk())
                    return {status, storage_type::InvalidEntryId};

            auto children = entry.children();
            bool found{false};

            for (auto child : children) {
                auto [name, id] = child;

                if (name == t) {
                    handle = id;

                    found = true;

                    break;
                }
            }

            if (!found)
                return {Status::InvalidArgument("No such entry"), storage_type::InvalidEntryId};

            reconstructedPath += ("/" + t);

            updatePathCacheEntry(reconstructedPath, handle);
        }

        return {Status::Ok(), handle};
    }

    Status close(Volume::Handle d)  {
        // do nothing for now

        static_cast<void>(d);

        return Status::Ok();
    }

    std::tuple<Status, std::set<std::string>> children(Volume::Handle handle) {
        std::shared_lock locker(mutexForHandle(handle));

        auto [status, entry] = storage_->load(handle);

        if (!status.isOk())
            return {status, {}};

        auto children = entry.children();
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
       std::shared_lock locker(mutexForHandle(handle));

       auto [status, entry] = storage_->load(handle);

       if (!status.isOk())
            return {status, {}};

       return {Status::Ok(), entry.propertiesSet()};
    }

    std::tuple<Status, Property> property(Volume::Handle handle, std::string_view name) {
        std::shared_lock locker(mutexForHandle(handle));

        auto [status, entry] = storage_->load(handle);

        if (!status.isOk())
            return {status, {}};

        return entry.property(util::to_string(name));
    }

    Status setProperty(Volume::Handle handle, std::string_view name, const Property &value) {
        std::unique_lock locker(mutexForHandle(handle));

        auto [status, entry] = storage_->load(handle);

        if (!status.isOk())
            return status;

        status = entry.setProperty(util::to_string(name), value);

        if (!status.isOk())
            return status;

        // TODO: DO NOT SAVE ON EACH ENTRY UPDATE!!!!
        return storage_->save(entry);
    }

    Status removeProperty(Volume::Handle handle, std::string_view name) {
        std::unique_lock locker(mutexForHandle(handle));

        auto [status, entry] = storage_->load(handle);

        if (!status.isOk())
            return status;

        status = entry.removeProperty(util::to_string(name));

        if (!status.isOk())
            return status;

        // TODO: DO NOT SAVE ON EACH ENTRY UPDATE!!!!
        return storage_->save(entry);
    }

    std::tuple<Status, bool> hasProperty(Volume::Handle handle, std::string_view name) {
        std::shared_lock locker(mutexForHandle(handle));

        auto [status, entry] = storage_->load(handle);

        if (!status.isOk())
            return {status, false};

        return {Status::Ok(), entry.hasProperty(util::to_string(name))};
    }

    Status expireProperty(Volume::Handle handle, std::string_view name, chrono::system_clock::time_point tp) {
        std::unique_lock locker(mutexForHandle(handle));

        auto [status, entry] = storage_->load(handle);

        if (!status.isOk())
            return status;

        status = entry.expireProperty(util::to_string(name), tp);

        if (!status.isOk())
            return status;

        // TODO: DO NOT SAVE ON EACH ENTRY UPDATE!!!!
        return storage_->save(entry);
    }

    Status cancelPropertyExpiration(Volume::Handle handle, std::string_view name) {
        std::unique_lock locker(mutexForHandle(handle));

        auto [status, entry] = storage_->load(handle);

        if (!status.isOk())
            return status;

        status = entry.cancelPropertyExpiration(util::to_string(name));

        if (!status.isOk())
            return status;

        // TODO: DO NOT SAVE ON EACH ENTRY UPDATE!!!!
        return storage_->save(entry);
    }

    std::tuple<Status, Volume::Handle> createChild(Volume::Handle handle, std::string_view name) {
        //TODO: check name for validness

        std::unique_lock locker(mutexForHandle(handle));

        auto [status, entry] = storage_->load(handle);

        if (!status.isOk())
            return {status, storage_type::InvalidEntryId};

        storage_type::entry_type child{storage_->newKey(), util::to_string(name)};

        status = entry.addChild(child);

        if (!status.isOk()) {
            storage_->reuseKey(child.key());

            return {status, storage_type::InvalidEntryId};
        }

        // TODO: DO NOT SAVE ON EACH ENTRY UPDATE!!!!
        status = storage_->save(child);

        if (!status.isOk())
            return {status, storage_type::InvalidEntryId};

        // TODO: DO NOT SAVE ON EACH ENTRY UPDATE!!!!
        status = storage_->save(entry);

        if (!status.isOk()) {
            assert(storage_->remove(child).isOk());

            return {status, storage_type::InvalidEntryId};
        }

        return {Status::Ok(), child.key()};
    }

    Status unlink(std::string_view path) {
        invalidatePathCacheEntry(util::to_string(path));

        // TODO: implement

        return Status::Ok();
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

    std::shared_mutex& mutexForHandle(Volume::Handle h) noexcept {
        static constexpr std::hash<Volume::Handle> hasher;

        auto idx = hasher(h) % MAX_THREADS;

        return xLocks_[idx];
    }

    std::unique_ptr<storage_type> storage_;
    MRUCache<std::string,Volume::Handle, 1024> pathCache_;
    std::array<std::shared_mutex, MAX_THREADS> xLocks_;
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

    return impl_->storage_->open(directory, volumeName);
}

Status Volume::deinitialize() {
    if (!initialized())
        return Status::InvalidOperation("Volume not opened");

    return impl_->storage_->close();
}

bool Volume::initialized() const noexcept {
    return impl_->storage_->opened();
}

std::tuple<Status, Volume::Handle> Volume::open(std::string_view path) {
    if (!initialized())
        return {Status::InvalidOperation("Volume not opened"), Impl::storage_type::InvalidEntryId};

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

std::tuple<Status, Volume::Handle> Volume::link(Volume::Handle h, std::string_view name) {
    if (!initialized())
        return {Status::InvalidOperation("Volume not opened"), Impl::storage_type::InvalidEntryId};

    return impl_->createChild(h, name);
}

Status Volume::unlink(std::string_view path) {
    if (!initialized())
        return Status::InvalidOperation("Volume not opened");

    return impl_->unlink(path);
}


}
