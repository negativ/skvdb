#include "MountPointEntry.hpp"

#include "util/String.hpp"
#include "util/StringPath.hpp"
#include "util/Unused.hpp"

namespace skv::vfs::mount {

struct Entry::Impl {
    Impl(std::string mp, std::string ep, IVolumePtr volume, IVolume::Handle handle, Entry::Priority prio):
        mountPath_(std::move(mp)),
        entryPath_(std::move(ep)),
        volume_(std::move(volume)),
        handle_(handle),
        priority_(prio)
    {

    }
    ~Impl() noexcept  = default;

    Impl(const Impl&) = default;
    Impl& operator=(const Impl&) = default;

    Impl(Impl&&) noexcept = default;
    Impl& operator=(Impl&&) noexcept = default;

    std::string mountPath_;
    std::string entryPath_;
    IVolumePtr volume_;
    IVolume::Handle handle_;
    Entry::Priority priority_;
};

Entry::Entry(std::string_view mountPath, std::string_view entryPath, IVolumePtr volume, Priority prio):
    impl_{std::make_unique<Impl>(util::simplifyPath(mountPath),
                                 util::simplifyPath(entryPath),
                                 std::move(volume),
                                 IVolume::InvalidHandle,
                                 prio)}
{

}

Entry::~Entry() noexcept = default;

Entry::Entry(const Entry& other) {
    using std::swap;

    auto copy = std::make_unique<Impl>(*other.impl_);

    swap(impl_, copy);
}

Entry& Entry::operator=(const Entry& other) {
    using std::swap;

    auto copy = std::make_unique<Impl>(*other.impl_);

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

Entry::Priority Entry::priority() const noexcept {
    return impl_->priority_;
}

bool Entry::open() {
    if (impl_->volume_) {
        auto [status, handle] = impl_->volume_->open(impl_->entryPath_);

        if (status.isOk())
            impl_->handle_ = handle;

        return status.isOk();
    }

    return false;
}

void Entry::close() {
    if (opened())
        SKV_UNUSED(impl_->volume_->close(impl_->handle_));
}

bool Entry::opened() const noexcept {
    return impl_->handle_ != IVolume::InvalidHandle;
}

}
