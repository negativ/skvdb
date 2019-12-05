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

Volume::Volume(Volume &&other) noexcept{
    std::swap(impl_, other.impl_);
}

Volume::~Volume() noexcept = default;

Status Volume::initialize(const std::string &directory, const std::string &volumeName) {
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

std::shared_ptr<IEntry> Volume::entry(const std::string& path) {
    if (!initialized())
        return {};

    return impl_->entry(path);
}

Status Volume::link(IEntry &entry, std::string_view name) {
    if (!initialized())
        return VolumeNotOpenedStatus;

    return impl_->createChild(entry, name);
}

Status Volume::unlink(IEntry& entry, std::string_view name) {
    if (!initialized())
        return VolumeNotOpenedStatus;

    return impl_->removeChild(entry, name);
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
