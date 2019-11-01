#include "Volume.hpp"
#include "Volume_p.hpp"

namespace skv::ondisk {

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

std::tuple<Status, Volume::Links> Volume::links(Volume::Handle h) {
    if (!initialized())
        return {Status::InvalidOperation("Volume not opened"), {}};

    return impl_->children(h);
}

std::tuple<Status, Volume::Properties > Volume::properties(Volume::Handle handle) {
    if (!initialized())
        return {Status::InvalidOperation("Volume not opened"), {}};

    return impl_->properties(handle);
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

Status Volume::claim(IVolume::Token token) noexcept {

}

Status Volume::release(IVolume::Token token) noexcept {

}


}
