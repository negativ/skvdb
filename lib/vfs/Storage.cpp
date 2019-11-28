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

std::tuple<Status, Storage::Handle> Storage::open(std::string_view path) {
    return impl_->open(path);
}

Status Storage::close(Storage::Handle handle) {
    return impl_->close(handle);
}

std::tuple<Status, Storage::Properties > Storage::properties(Storage::Handle handle) {
    return impl_->properties(handle);
}

std::tuple<Status, Storage::PropertiesNames> Storage::propertiesNames(Handle handle) {
    return impl_->propertiesNames(handle);
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
    return impl_->expireProperty(handle, name, tp);
}

Status Storage::cancelPropertyExpiration(Storage::Handle handle, std::string_view name) {
    return impl_->cancelPropertyExpiration(handle, name);
}

std::tuple<Status, Storage::Links> Storage::links(Storage::Handle handle) {
    return impl_->links(handle);
}

Status Storage::link(Storage::Handle handle, std::string_view name) {
    return impl_->link(handle, name);
}

Status Storage::unlink(Handle handle, std::string_view name) {
    return impl_->unlink(handle, name);
}

Status Storage::mount(const IVolumePtr& volume, std::string_view entryPath, std::string_view mountPath, Storage::Priority prio) {
    return impl_->mount(volume, entryPath, mountPath, prio);
}

Status Storage::unmount(const IVolumePtr& volume, std::string_view entryPath, std::string_view mountPath) {
    return impl_->unmount(volume, entryPath, mountPath);
}

}

