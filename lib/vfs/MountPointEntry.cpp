#include "MountPointEntry.hpp"

#include "util/String.hpp"
#include "util/StringPath.hpp"
#include "util/Unused.hpp"

namespace skv::vfs::mount {

struct Entry::Impl {
    std::string mountPath_;
    std::string entryPath_;
    IVolumePtr volume_;
    IVolume::Handle handle_;
};

Entry::Entry(std::string_view mountPath, std::string_view entryPath, IVolumePtr volume):
    impl_{new Impl{util::simplifyPath(mountPath),
                   util::simplifyPath(entryPath),
                   volume,
                   IVolume::InvalidHandle}}
{
    if (volume) {
        auto [status, handle] = volume->open(impl_->entryPath_);

        if (status.isOk())
            impl_->handle_ = handle;
    }
}

Entry::~Entry() noexcept {
    if (valid())
        SKV_UNUSED(impl_->volume_->close(impl_->handle_));
}

Entry::Entry(const Entry& other) {
    using std::swap;

    auto copy = std::make_unique<Impl>();
    *copy = *other.impl_;

    swap(impl_, copy);
}

Entry& Entry::operator=(const Entry& other) {
    using std::swap;

    auto copy = std::make_unique<Impl>();
    *copy = *other.impl_;

    swap(impl_, copy);

    return *this;
}

Entry::Entry(Entry&& other) noexcept {
    using std::swap;

    swap(impl_, other.impl_);
}

Entry& Entry::operator=(Entry&& other) noexcept{
    using std::swap;

    swap(impl_, other.impl_);

    return *this;
}

std::string Entry::mountPath() const {
    return impl_->mountPath_;
}

std::string Entry::entryPath() const
{
    return impl_->entryPath_;
}

IVolumePtr Entry::volume() const {
    return impl_->volume_;
}

IVolume::Handle Entry::handle() const noexcept {
    return impl_->handle_;
}

bool Entry::valid() const noexcept {
    return impl_->handle_ != IVolume::InvalidHandle;
}

}
