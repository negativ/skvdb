#include "Storage.hpp"
#include "Storage_p.hpp"

#include "util/ExceptionBoundary.hpp"
#include "util/Log.hpp"

namespace {

constexpr auto NotConstructedStatus  = skv::util::Status::Fatal("Not constructed");

}

namespace skv::vfs {

using namespace skv::util;

Storage::Storage(Status& status) noexcept {
    auto r = exceptionBoundary("Storage::Storage",
                               [&] {
                                   auto ptr = std::make_unique<Impl>();
                                   impl_.swap(ptr);

                                   status = Status::Ok();
                               });

    if (!r.isOk())
        status = r;
}

Storage::Storage(Storage &&other) noexcept {
    std::swap(impl_, other.impl_);
}

std::shared_ptr<IEntry> Storage::entry(const std::string &path) {
    if (!impl_)
        return {};

    std::shared_ptr<IEntry> ret;
    exceptionBoundary("Storage::entry",
                      [&] {
                          ret = impl_->entry(path);
                      });

    return ret;
}

Storage::~Storage() noexcept = default;

Status Storage::link(IEntry &entry, const std::string& name) {
    if (!impl_)
        return NotConstructedStatus;

    Status ret;
    auto status = exceptionBoundary("Storage::link",
                                    [&] {
                                        ret = impl_->link(entry, name);
                                    });

    return status.isOk()? ret : status;
}

Status Storage::unlink(IEntry &entry, const std::string& name) {
    if (!impl_)
        return NotConstructedStatus;

    Status ret;
    auto status = exceptionBoundary("Storage::unlink",
                                    [&] {
                                        ret = impl_->unlink(entry, name);
                                    });

    return status.isOk()? ret : status;
}

Status Storage::claim(IVolume::Token token) noexcept {
    if (!impl_)
        return NotConstructedStatus;

    return impl_->claim(token);
}

Status Storage::release(IVolume::Token token) noexcept {
    if (!impl_)
        return NotConstructedStatus;

    return impl_->release(token);
}

Status Storage::mount(const std::shared_ptr<IVolume>& volume, const std::string& entryPath, const std::string& mountPath, Storage::Priority prio) {
    if (!impl_)
        return NotConstructedStatus;

    if (volume.get() == static_cast<IVolume*>(this))
        return Status::InvalidOperation("Invalid volume");

    Status ret;
    auto status = exceptionBoundary("Storage::mount",
                                    [&] {
                                        ret = impl_->mount(volume, entryPath, mountPath, prio);
                                    });

    return status.isOk()? ret : status;
}

Status Storage::unmount(const std::shared_ptr<IVolume>& volume, const std::string& entryPath, const std::string& mountPath) {
    if (!impl_)
        return NotConstructedStatus;

    Status ret;
    auto status = exceptionBoundary("Storage::unmount",
                                    [&] {
                                        ret = impl_->unmount(volume, entryPath, mountPath);
                                    });

    return status.isOk()? ret : status;
}

}

