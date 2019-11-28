#include "Volume.hpp"
#include "Volume_p.hpp"

namespace {

const skv::util::Status VolumeNotOpenedStatus = skv::util::Status::InvalidOperation("Volume not opened");

}

namespace skv::ondisk {

Volume::Volume():
    Volume(OpenOptions{})
{

}

Volume::Volume(OpenOptions opts):
    impl_{std::make_unique<Impl>(opts)}
{

}

Volume::~Volume() noexcept = default;

Status Volume::initialize(std::string_view directory, std::string_view volumeName) {
    if (initialized())
        return Status::InvalidOperation("Volume already opened");

    return impl_->initialize(directory, volumeName);
}

Status Volume::deinitialize() {
    if (!initialized())
        return VolumeNotOpenedStatus;

    return impl_->deinitialize();
}

bool Volume::initialized() const noexcept {
    return impl_->initialized();
}

std::tuple<Status, Volume::Handle> Volume::open(std::string_view path) {
    if (!initialized())
        return {VolumeNotOpenedStatus, Volume::InvalidHandle};

    return impl_->open(path);
}

Status Volume::close(Volume::Handle handle) {
    if (!initialized())
        return VolumeNotOpenedStatus;

    return impl_->close(handle);
}

std::tuple<Status, Volume::Links> Volume::links(Volume::Handle h) {
    if (!initialized())
        return {VolumeNotOpenedStatus, {}};

    return impl_->children(h);
}

std::tuple<Status, Volume::Properties > Volume::properties(Volume::Handle handle) {
    if (!initialized())
        return {VolumeNotOpenedStatus, {}};

    return impl_->properties(handle);
}
std::tuple<Status, Volume::PropertiesNames> Volume::propertiesNames(Handle handle) {
    if (!initialized())
        return { VolumeNotOpenedStatus, {} };

    return impl_->propertiesNames(handle);
}

std::tuple<Status, Property> Volume::property(Volume::Handle h, std::string_view name) {
    if (!initialized())
        return {VolumeNotOpenedStatus, {}};

    return impl_->property(h, name);
}

Status Volume::setProperty(Volume::Handle h, std::string_view name, const Property &value) {
    if (!initialized())
        return VolumeNotOpenedStatus;

    return impl_->setProperty(h, name, value);
}

Status Volume::removeProperty(Volume::Handle h, std::string_view name) {
    if (!initialized())
        return VolumeNotOpenedStatus;

    return impl_->removeProperty(h, name);
}

std::tuple<Status, bool> Volume::hasProperty(Volume::Handle h, std::string_view name) {
    if (!initialized())
        return {VolumeNotOpenedStatus, false};

    return impl_->hasProperty(h, name);
}

Status Volume::expireProperty(Volume::Handle h, std::string_view name, chrono::system_clock::time_point tp) {
    if (!initialized())
        return VolumeNotOpenedStatus;

    return impl_->expireProperty(h, name, tp);
}

Status Volume::cancelPropertyExpiration(Volume::Handle handle, std::string_view name) {
    if (!initialized())
        return VolumeNotOpenedStatus;

    return impl_->cancelPropertyExpiration(handle, name);
}

Status Volume::link(Volume::Handle handle, std::string_view name) {
    if (!initialized())
        return VolumeNotOpenedStatus;

    return impl_->createChild(handle, name);
}

Status Volume::unlink(Handle h, std::string_view name) {
    if (!initialized())
        return VolumeNotOpenedStatus;

    return impl_->removeChild(h, name);
}

Status Volume::claim(IVolume::Token token) noexcept {
    if (!initialized())
        return VolumeNotOpenedStatus;

    return impl_->claim(token);
}

Status Volume::release(IVolume::Token token) noexcept {
    if (!initialized())
        return VolumeNotOpenedStatus;

    return impl_->release(token);
}

}
