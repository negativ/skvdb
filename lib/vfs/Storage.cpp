#include "Storage.hpp"

#include <algorithm>
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

        entries.insert(std::lower_bound(std::cbegin(entries), std::cend(entries), entry), entry);

        return  true;
    }

    bool hasVirtualEntries(Storage::Handle handle) const {
        auto it = ventries_.find(handle);

        return (it != std::cend(ventries_));
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
    std::string vpath = simplifyPath(path);
    auto [status, mountPath] = impl_->searchMountPathFor(vpath);

    if (!status.isOk())
        return {status, Storage::InvalidHandle};

    auto& index = impl_->mpoints_.get<mount::tags::ByMountPath>();
    auto [start, stop] = index.equal_range(util::to_string(mountPath));

    auto subvpath = vpath.substr(mountPath.size(), vpath.size() - mountPath.size()); // extracting subpath from vpath

    Storage::Handle vhandle = impl_->newHandle();

    std::for_each(start, stop,
                  [&](auto&& entry) {
                      auto volume = entry.volume();
                      auto subpath = simplifyPath(entry.entryPath() + "/" + subvpath);
                      auto [status, handle] = volume->open(subpath);

                      if (status.isOk())
                          impl_->addVirtualEntry(vhandle, VirtualEntry(subpath, volume, handle, entry.priority()));
                  });

    if (!impl_->hasVirtualEntries(vhandle))
        return {Status::InvalidArgument("No such path"), Storage::InvalidHandle};

    return {Status::Ok(), vhandle};
}

Status Storage::close(Storage::Handle handle) {
    if (!impl_->hasVirtualEntries(handle))
        return Status::InvalidArgument("No such entry");

    // TODO: implement

    return Status::Ok();
}

std::tuple<Status, Storage::Links> Storage::links(Storage::Handle handle) {

}

std::tuple<Status, Storage::Properties > Storage::properties(Storage::Handle handle) {

}

std::tuple<Status, Property> Storage::property(Storage::Handle handle, std::string_view name) {

}

Status Storage::setProperty(Storage::Handle handle, std::string_view name, const Property &value) {

}

Status Storage::removeProperty(Storage::Handle handle, std::string_view name) {

}

std::tuple<Status, bool> Storage::hasProperty(Storage::Handle handle, std::string_view name) {

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

