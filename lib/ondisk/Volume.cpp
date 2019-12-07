#include "Volume.hpp"
#include "Volume_p.hpp"
#include "util/Log.hpp"

namespace {

const skv::util::Status VolumeNotOpenedStatus = skv::util::Status::InvalidOperation("Volume not opened");
const char * const TAG = "ondisk::Volume";

}

namespace skv::ondisk {

using namespace skv::util;

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

Status Volume::initialize(const os::path& directory, const std::string &volumeName) {
    if (initialized())
        return Status::InvalidOperation("Volume already opened");

    try {
        return impl_->initialize(directory, volumeName);
    }
    catch (const std::bad_alloc&) {
        // it's not safe to call any function, just return
    }
    catch (const std::exception& e) {
        Log::e(TAG, "ondisk::Volume::initialize(): got exception: ", e.what());
    }
    catch (...) {
        Log::e(TAG, "ondisk::Volume::initialize(): unknown exception");
    }
    return Status::Fatal("Exception");
}

Status Volume::deinitialize() {
    if (!initialized())
        return VolumeNotOpenedStatus;

    try {
        return impl_->deinitialize();
    }
    catch (const std::bad_alloc&) {
        // it's not safe to call any function, just return
    }
    catch (const std::exception& e) {
        Log::e(TAG, "ondisk::Volume::deinitialize(): got exception: ", e.what());
    }
    catch (...) {
        Log::e(TAG, "ondisk::Volume::deinitialize(): unknown exception");
    }

    return Status::Fatal("Exception");
}

bool Volume::initialized() const noexcept {
    return impl_->initialized();
}

std::shared_ptr<IEntry> Volume::entry(const std::string& path) {
    if (!initialized())
        return {};

    try {
        return impl_->entry(path);
    }
    catch (const std::bad_alloc&) {
        // it's not safe to call any function, just return empty ptr and pray (shared_ptr should have noexcept default constructor)
    }
    catch (const std::exception& e) {
        Log::e(TAG, "ondisk::Volume::entry(): got exception: ", e.what());
    }
    catch (...) {
        Log::e(TAG, "ondisk::Volume::entry(): unknown exception");
    }

    return {};
}

Status Volume::link(IEntry &entry, const std::string& name) {
    if (!initialized())
        return VolumeNotOpenedStatus;

    try {
        return impl_->createChild(entry, name);
    }
    catch (const std::bad_alloc&) {
        // it's not safe to call any function, just return
    }
    catch (const std::exception& e) {
        Log::e(TAG, "ondisK::Volume::link(): got exception: ", e.what());
    }
    catch (...) {
        Log::e(TAG, "ondisK::Volume::link(): unknown exception");
    }

    return Status::Fatal("Exception");
}

Status Volume::unlink(IEntry& entry, const std::string& name) {
    if (!initialized())
        return VolumeNotOpenedStatus;

    try {
        return impl_->removeChild(entry, name);
    }
    catch (const std::bad_alloc&) {
        // it's not safe to call any function, just return
    }
    catch (const std::exception& e) {
        Log::e(TAG, "ondisK::Volume::unlink(): got exception: ", e.what());
    }
    catch (...) {
        Log::e(TAG, "ondisK::Volume::unlink(): unknown exception");
    }

    return Status::Fatal("Exception");
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
