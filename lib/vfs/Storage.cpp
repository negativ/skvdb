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

Status Storage::link(IEntry &entry, std::string_view name) {
    return impl_->link(entry, name);
}

Status Storage::unlink(IEntry &entry, std::string_view name) {
    return impl_->unlink(entry, name);
}

Status Storage::claim(IVolume::Token token) noexcept {
	return Status::IOError(""); // TODO: implement
}

Status Storage::release(IVolume::Token token) noexcept {
	return Status::IOError(""); // TODO: implement
}

Status Storage::mount(const IVolumePtr& volume, std::string_view entryPath, std::string_view mountPath, Storage::Priority prio) {
    return impl_->mount(volume, entryPath, mountPath, prio);
}

Status Storage::unmount(const IVolumePtr& volume, std::string_view entryPath, std::string_view mountPath) {
    return impl_->unmount(volume, entryPath, mountPath);
}

}

