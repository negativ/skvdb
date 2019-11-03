#include "Storage.hpp"

#include <algorithm>
#include <future>
#include <thread>
#include <type_traits>
#include <unordered_map>

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

        Storage::Handle vhandle = newHandle();
        std::vector<future_result_t> results;

        std::transform(start, stop,
                       std::back_inserter(results),
                       [&](auto&& entry) {
                           return std::async(std::launch::async, [=]() -> result_t {
                               auto volume = entry.volume();
                               auto subpath = simplifyPath(entry.entryPath() + "/" + subvpath);
                               auto [status, handle] = volume->open(subpath);

                               if (status.isOk())
                                   return {status, VirtualEntry(subpath, volume, handle, entry.priority())};

                               return {Status::InvalidArgument("Invalid volume"), VirtualEntry{}};
                           });
                       });

        for (auto& result: results) {
            auto [status, ventry] = result.get();

            if (status.isOk())
                addVirtualEntry(vhandle, ventry);
        }

        if (!hasVirtualEntries(vhandle))
            return {Status::InvalidArgument("No such path"), Storage::InvalidHandle};

        return {Status::Ok(), vhandle};
    }

    Status close(Storage::Handle handle) {
        using result_t = Status;
        using future_result_t = std::future<result_t>;

        if (hasVirtualEntries(handle))
            return Status::InvalidArgument("No such entry");

        auto& entries = ventries_[handle];
        std::vector<future_result_t> results;

        std::transform(std::begin(entries), std::end(entries),
                       std::back_inserter(results),
                       [&](auto&& entry) {
                           return std::async(std::launch::async, [=]() -> result_t {
                               auto volume = entry.volume();

                               if (auto volume = entry.volume().lock(); volume)
                                   return volume->close(entry.handle());

                               return Status::InvalidArgument("Invalid volume");
                           });
                       });

        bool ret = true;

        for (auto& result: results) {
            auto status = result.get();

            ret = ret &&  status.isOk();
        }

        removeVirtualEntry(handle);

        return ret? Status::Ok() : Status::InvalidOperation("Unable to close handle of all volumes");
    }

    std::tuple<Status, Storage::Properties > properties(Storage::Handle handle) {
        return {Status::Ok(), {}};
    }

    std::tuple<Status, Property> property(Storage::Handle handle, std::string_view name) {
        return {Status::Ok(), {}};
    }

    Status setProperty(Storage::Handle handle, std::string_view name, const Property &value) {
        return Status::Ok();
    }

    Status removeProperty(Storage::Handle handle, std::string_view name) {
        return Status::Ok();
    }

    std::tuple<Status, Storage::Links> links(Storage::Handle handle) {
        return {Status::Ok(), {}};
    }

    std::tuple<Status, bool> hasProperty(Storage::Handle handle, std::string_view name) {
        return {Status::Ok(), {}};
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

    bool addVirtualEntry(Storage::Handle handle, VirtualEntry entry) {
        auto& entries = ventries_[handle];

        entries.insert(std::lower_bound(std::cbegin(entries), std::cend(entries),
                                        entry, std::greater<VirtualEntry>()),
                       entry);

        return  true;
    }

    bool hasVirtualEntries(Storage::Handle handle) const {
        auto it = ventries_.find(handle);

        return (it != std::cend(ventries_));
    }

    void removeVirtualEntry(Storage::Handle handle) {
        auto it = ventries_.find(handle);

        if (it != std::end(ventries_))
            ventries_.erase(it);
    }

    Storage::Handle newHandle() noexcept {
        return ++currentHandle_;
    }

    mount::Points mpoints_{};
    std::unordered_map<Storage::Handle, VirtualEntries> ventries_;
    Storage::Handle currentHandle_{1};
};

Storage::Storage():
    impl_{new Impl{}}
{

}

Storage::~Storage() noexcept {

}

std::tuple<Status, Storage::Handle> Storage::open(std::string_view path) {
    return impl_->open(path);
}

Status Storage::close(Storage::Handle handle) {
    return impl_->close(handle);
}

std::tuple<Status, Storage::Links> Storage::links(Storage::Handle handle) {
    return impl_->links(handle);
}

std::tuple<Status, Storage::Properties > Storage::properties(Storage::Handle handle) {
    return impl_->properties(handle);
}

std::tuple<Status, Property> Storage::property(Storage::Handle handle, std::string_view name) {
    return impl_->property(handle, name);
}

Status Storage::setProperty(Storage::Handle handle, std::string_view name, const Property &value) {
    return impl_->setProperty(handle, name, value);
}

Status Storage::removeProperty(Storage::Handle handle, std::string_view name) {
    return impl_->removeProperty(handle, name);
}

std::tuple<Status, bool> Storage::hasProperty(Storage::Handle handle, std::string_view name) {
    return impl_->hasProperty(handle, name);
}

Status Storage::expireProperty(Storage::Handle handle, std::string_view name, chrono::system_clock::time_point tp) {

}

Status Storage::cancelPropertyExpiration(Storage::Handle handle, std::string_view name) {

}

Status Storage::link(Storage::Handle handle, std::string_view name) {

}

Status Storage::unlink(Handle handle, std::string_view name) {

}

Status Storage::mount(IVolumePtr volume, std::string_view entryPath, std::string_view mountPath, Storage::Priority prio) {
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

    auto& index = impl_->mpoints_.get<mount::tags::ByAll>();
    auto retp = index.insert(std::move(entry));

    if (!retp.second) {
        SKV_UNUSED(volume->release(this));
        entry.close();

        return Status::InvalidArgument("Mount point entry already exist");
    }

    return Status::Ok();
}

Status Storage::unmount(IVolumePtr volume, std::string_view entryPath, std::string_view mountPath) {
    if (!volume)
        return InvalidVolumeArgumentStatus;

    // TODO: protect by mutex

    auto& index = impl_->mpoints_.get<mount::tags::ByMountPath>();
    auto [start, stop] = index.equal_range(util::to_string(mountPath));
    auto entryIt = std::find_if(start, stop,
                                [&](auto&& entry) {
                                    return entry.volume() == volume && entry.entryPath() == entryPath;
                                });

    if (entryIt == std::end(index))
        return Status::InvalidArgument("No such mount point entry");

    if (!volume->release(this).isOk())
        return Status::Fatal("Unable to release volume");

    mount::Entry e(std::move(*entryIt));
    e.close();

    index.erase(entryIt);

    return Status::Ok();
}

}

