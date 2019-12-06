#include "MountPointEntry.hpp"

#include "util/String.hpp"
#include "util/StringPath.hpp"
#include "util/Unused.hpp"

namespace skv::vfs::mount {

struct Entry::Impl {
    Impl(std::string mp, std::string ep, IVolumePtr volume, Entry::Priority prio):
        mountPath_(std::move(mp)),
        entryPath_(std::move(ep)),
        volume_(std::move(volume)),
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
    std::shared_ptr<vfs::IEntry> entry_;
    Entry::Priority priority_;
};

Entry::Entry(const std::string& mountPath, const std::string& entryPath, IVolumePtr volume, Priority prio):
    impl_{std::make_unique<Impl>(util::simplifyPath(mountPath),
                                 util::simplifyPath(entryPath),
                                 std::move(volume),
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

std::shared_ptr<IEntry> Entry::entry() const {
    return impl_->entry_;
}

Entry::Priority Entry::priority() const noexcept {
    return impl_->priority_;
}

bool Entry::open() {
    if (impl_->volume_ && impl_->entry_ == nullptr) {
        impl_->entry_ = impl_->volume_->entry(impl_->entryPath_);

        if (impl_->entry_ != nullptr)
            return true;
    }

    return false;
}

void Entry::close() {
    impl_->entry_.reset();
}

bool Entry::opened() const noexcept {
    return impl_->entry_ != nullptr;
}

bool Entry::operator<(const Entry& other) const noexcept {
	return priority() < other.priority();
}

bool Entry::operator>(const Entry& other) const noexcept {
	return priority() > other.priority();
}

}
