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

Storage::~Storage() noexcept = default;

Status Storage::link(Storage::Handle handle, std::string_view name) {
//    return impl_->link(handle, name);
}

Status Storage::unlink(Handle handle, std::string_view name) {
//    return impl_->unlink(handle, name);
}

Status Storage::mount(const IVolumePtr& volume, std::string_view entryPath, std::string_view mountPath, Storage::Priority prio) {
//    return impl_->mount(volume, entryPath, mountPath, prio);
}

Status Storage::unmount(const IVolumePtr& volume, std::string_view entryPath, std::string_view mountPath) {
//    return impl_->unmount(volume, entryPath, mountPath);
}

}

