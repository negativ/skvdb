#include "Storage.hpp"
#include "Storage_p.hpp"

#include <stdexcept>

#include "util/Log.hpp"

namespace {
    const char * const TAG = "vfs::Storage";
}

namespace skv::vfs {

using namespace skv::util;

Storage::Storage(Status& status) noexcept try:
    impl_{std::make_unique<Impl>()}
{
    status = Status::Ok();
}
catch (...)
{
    status = Status::Fatal("Exception");
}

Storage::Storage(Storage &&other) noexcept {
    std::swap(impl_, other.impl_);
}

std::shared_ptr<IEntry> Storage::entry(const std::string &path) {
    if (!impl_)
        return {};

    try {
        return impl_->entry(path);
    }
    catch (const std::bad_alloc&) {
        // it's not safe to call any function, just return empty ptr and pray (shared_ptr should have noexcept default constructor)
    }
    catch (const std::exception& e) {
        Log::e(TAG, "vfs::Storage::entry(): got exception: ", e.what());
    }
    catch (...) {
        Log::e(TAG, "vfs::Storage::entry(): unknown exception");
    }

    return {};
}

Storage::~Storage() noexcept = default;

Status Storage::link(IEntry &entry, const std::string& name) {
    if (!impl_)
        return Status::Fatal("Not constructed");

    try {
        return impl_->link(entry, name);
    }
    catch (const std::bad_alloc&) {
        // it's not safe to call any function, just return
    }
    catch (const std::exception& e) {
        Log::e(TAG, "vfs::Storage::link(): got exception: ", e.what());
    }
    catch (...) {
        Log::e(TAG, "vfs::Storage::link(): unknown exception");
    }

    return Status::Fatal("Exception");
}

Status Storage::unlink(IEntry &entry, const std::string& name) {
    if (!impl_)
        return Status::Fatal("Not constructed");

    try {
        return impl_->unlink(entry, name);
    }
    catch (const std::bad_alloc&) {
        // it's not safe to call any function, just return
    }
    catch (const std::exception& e) {
        Log::e(TAG, "vfs::Storage::unlink(): got exception: ", e.what());
    }
    catch (...) {
        Log::e(TAG, "vfs::Storage::unlink(): unknown exception");
    }

    return Status::Fatal("Exception");
}

Status Storage::claim(IVolume::Token token) noexcept {
    if (!impl_)
        return Status::Fatal("Not constructed");

    return impl_->claim(token);
}

Status Storage::release(IVolume::Token token) noexcept {
    if (!impl_)
        return Status::Fatal("Not constructed");

    return impl_->release(token);
}

Status Storage::mount(const IVolumePtr& volume, const std::string& entryPath, const std::string& mountPath, Storage::Priority prio) {
    if (!impl_)
        return Status::Fatal("Not constructed");

    if (volume.get() == static_cast<IVolume*>(this))
        return Status::InvalidOperation("Invalid volume");

    try {
        return impl_->mount(volume, entryPath, mountPath, prio);
    }
    catch (const std::bad_alloc&) {
        // it's not safe to call any function, just return
    }
    catch (const std::exception& e) {
        Log::e(TAG, "vfs::Storage::mount(): got exception: ", e.what());
    }
    catch (...) {
        Log::e(TAG, "vfs::Storage::mount(): unknown exception");
    }

    return Status::Fatal("Exception");
}

Status Storage::unmount(const IVolumePtr& volume, const std::string& entryPath, const std::string& mountPath) {
    if (!impl_)
        return Status::Fatal("Not constructed");

    try {
        return impl_->unmount(volume, entryPath, mountPath);
    }
    catch (const std::bad_alloc&) {
        // it's not safe to call any function, just return
    }
    catch (const std::exception& e) {
        Log::e(TAG, "vfs::Storage::unmount(): got exception: ", e.what());
    }
    catch (...) {
        Log::e(TAG, "vfs::Storage::unmount(): unknown exception");
    }

    return Status::Fatal("Exception");
}

}

