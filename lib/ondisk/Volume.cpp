#include "Volume.hpp"
#include "Volume_p.hpp"
#include "util/Log.hpp"
#include "util/ExceptionBoundary.hpp"

namespace {

constexpr auto VolumeNotOpenedStatus = skv::util::Status::InvalidOperation("Volume not opened");

}

namespace skv::ondisk {

using namespace skv::util;

Volume::Volume(Status& status) noexcept:
    Volume(status, OpenOptions{})
{

}

Volume::Volume(Status& status, OpenOptions opts) noexcept {
    auto r = exceptionBoundary("Volume::Volume",
                               [&] {
                                    auto ptr = std::make_unique<Impl>(opts);
                                    impl_.swap(ptr);

                                    status = Status::Ok();
                               });

    if (!r.isOk())
        status = r;
}


Volume::Volume(Volume &&other) noexcept{
    std::swap(impl_, other.impl_);
}

Volume::~Volume() noexcept = default;

Status Volume::initialize(const os::path& directory, const std::string &volumeName) {
    if (initialized())
        return Status::InvalidOperation("Volume already opened");

    Status ret;
    auto status = exceptionBoundary("Volume::initialize",
                                    [&] {
                                        ret = impl_->initialize(directory, volumeName);
                                    });

    return status.isOk()? ret : status;
}

Status Volume::deinitialize() {
    if (!initialized())
        return VolumeNotOpenedStatus;

    Status ret;
    auto status = exceptionBoundary("Volume::deinitialize",
                                    [&] {
                                        ret = impl_->deinitialize();
                                    });

    return status.isOk()? ret : status;
}

bool Volume::initialized() const noexcept {
    return impl_ && impl_->initialized();
}

std::shared_ptr<IEntry> Volume::entry(const std::string& path) {
    if (!initialized())
        return {};

    std::shared_ptr<IEntry> ret;
    exceptionBoundary("Volume::entry",
                      [&] {
                          ret = impl_->entry(path);
                      });

    return ret;
}

Status Volume::link(IEntry &entry, const std::string& name) {
    if (!initialized())
        return VolumeNotOpenedStatus;

    Status ret;
    auto status = exceptionBoundary("Volume::link",
                                    [&] {
                                        ret = impl_->createChild(entry, name);
                                    });

    return status.isOk()? ret : status;
}

Status Volume::unlink(IEntry& entry, const std::string& name) {
    if (!initialized())
        return VolumeNotOpenedStatus;

    Status ret;
    auto status = exceptionBoundary("Volume::unlink",
                                    [&] {
                                        ret = impl_->removeChild(entry, name);
                                    });

    return status.isOk()? ret : status;
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
