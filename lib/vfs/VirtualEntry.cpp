#include "VirtualEntry.hpp"

#include "util/String.hpp"

namespace skv::vfs {

struct VirtualEntry::Impl {
    std::string entryPath_;
    IVolumeWPtr volume_;
    IVolume::Handle handle_;
    VirtualEntry::Priority priority_;
};

VirtualEntry::VirtualEntry(std::string_view entryPath, IVolumeWPtr volume, IVolume::Handle handle, VirtualEntry::Priority prio):
    impl_{new Impl{util::to_string(entryPath), volume, handle, prio}}
{

}

VirtualEntry::~VirtualEntry() noexcept = default;


VirtualEntry::VirtualEntry(const VirtualEntry& other) {
    using std::swap;

    auto copy = std::make_unique<Impl>(*other.impl_);

    swap(impl_, copy);
}

VirtualEntry& VirtualEntry::operator=(const VirtualEntry& other) {
    using std::swap;

    auto copy = std::make_unique<Impl>(*other.impl_);

    swap(impl_, copy);

    return *this;
}

VirtualEntry::VirtualEntry(VirtualEntry&& other) noexcept {
    using std::swap;

    swap(impl_, other.impl_);
}

VirtualEntry& VirtualEntry::operator=(VirtualEntry&& other) noexcept {
    using std::swap;

    swap(impl_, other.impl_);

    return *this;
}

std::string VirtualEntry::entryPath() const {
    return impl_->entryPath_;
}

IVolumeWPtr VirtualEntry::volume() const {
    return impl_->volume_;
}

IVolume::Handle VirtualEntry::handle() const noexcept {
    return impl_->handle_;
}

VirtualEntry::Priority VirtualEntry::priority() const noexcept {
    return impl_->priority_;
}

bool VirtualEntry::valid() const noexcept {
    return impl_ && !impl_->volume_.expired() && (impl_->handle_ != IVolume::InvalidHandle);
}

bool VirtualEntry::operator<(const VirtualEntry& other) const noexcept {
    return priority() < other.priority();
}

}
