#include "MountPointEntry.hpp"

#include "util/String.hpp"
#include "util/StringPath.hpp"
#include "util/Unused.hpp"

namespace skv::vfs {

struct MountPointEntry::Impl {
    std::string entryPath_;
    IVolumePtr volume_;
    IVolume::Handle handle_;
};

MountPointEntry::MountPointEntry(std::string_view entryPath, IVolumePtr volume):
    impl_{new Impl{util::simplifyPath(entryPath), volume, IVolume::InvalidHandle}}
{
    if (volume) {
        auto [status, handle] = volume->open(impl_->entryPath_);

        if (status.isOk())
            impl_->handle_ = handle;
    }
}

MountPointEntry::~MountPointEntry() noexcept {
    if (valid())
        SKV_UNUSED(impl_->volume_->close(impl_->handle_));
}

std::string MountPointEntry::entryPath() const
{
    return impl_->entryPath_;
}

IVolumePtr MountPointEntry::volume() const {
    return impl_->volume_;
}

IVolume::Handle MountPointEntry::handle() const noexcept {
    return impl_->handle_;
}

bool MountPointEntry::valid() const noexcept {
    return impl_->handle_ != IVolume::InvalidHandle;
}

}
