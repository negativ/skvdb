#include "Storage.hpp"
#include "Storage_p.hpp"

#include <stdexcept>

namespace skv::vfs {

Storage::Storage():
    impl_{std::make_unique<Impl>()}
{
}

Storage::Storage(Storage &&other) noexcept {
    std::swap(impl_, other.impl_);
}

std::shared_ptr<IEntry> Storage::entry(const std::string &path) {
	return impl_->entry(path);
}

Storage::~Storage() noexcept = default;

Status Storage::link(IEntry &entry, const std::string& name) {
    return impl_->link(entry, name);
}

Status Storage::unlink(IEntry &entry, const std::string& name) {
    return impl_->unlink(entry, name);
}

Status Storage::claim(IVolume::Token token) noexcept {
    return impl_->claim(token);
}

Status Storage::release(IVolume::Token token) noexcept {
    return impl_->release(token);
}

Status Storage::mount(const IVolumePtr& volume, const std::string& entryPath, const std::string& mountPath, Storage::Priority prio) {
    if (volume.get() == static_cast<IVolume*>(this))
        return Status::InvalidOperation("Invalid volume");

    return impl_->mount(volume, entryPath, mountPath, prio);
}

Status Storage::unmount(const IVolumePtr& volume, const std::string& entryPath, const std::string& mountPath) {
    return impl_->unmount(volume, entryPath, mountPath);
}

}

