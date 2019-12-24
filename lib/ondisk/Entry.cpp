#include "Entry.hpp"

namespace {
constexpr auto ExceptionThrownStatus = skv::util::Status::Fatal("Exception");
constexpr auto BadAllocThrownStatus  = skv::util::Status::Fatal("bad_alloc");
}

namespace skv::ondisk {

Entry::Entry(Record &&record) noexcept:
    record_{record}
{

}

IEntry::Handle Entry::handle() const noexcept {
    std::shared_lock locker{xLock_};

    return record_.handle();
}

std::tuple<Status, bool> Entry::hasProperty(const std::string &prop) const noexcept {
    std::shared_lock locker(xLock_);

    return {Status::Ok(), record_.hasProperty(prop)};
}

Status Entry::setProperty(const std::string &prop, const Property &value) {
    std::unique_lock locker{xLock_};

    try {
        auto status = record_.setProperty(prop, value);

        if (status.isOk())
            setDirty(true);

        return status;
    }
    catch (const std::bad_alloc&) { return BadAllocThrownStatus; }
    catch (...) { return ExceptionThrownStatus; }
}

std::tuple<Status, Property> Entry::property(const std::string &prop) const {
    std::shared_lock locker{xLock_};

    try {
        return record_.property(prop);
    }
    catch (const std::bad_alloc&) { return {BadAllocThrownStatus, {}}; }
    catch (...) { return {ExceptionThrownStatus, {}}; }
}

Status Entry::removeProperty(const std::string &prop) {
    std::unique_lock locker{xLock_};

    try {
        auto status = record_.removeProperty(prop);

        if (status.isOk())
            setDirty(true);

        return status;
    }
    catch (const std::bad_alloc&) { return BadAllocThrownStatus; }
    catch (...) { return ExceptionThrownStatus; }
}

std::tuple<Status, IEntry::Properties> Entry::properties() const {
    std::shared_lock locker{xLock_};

    try {
        return {Status::Ok(), record_.properties()};
    }
    catch (const std::bad_alloc&) { return {BadAllocThrownStatus, {}}; }
    catch (...) { return {ExceptionThrownStatus, {}}; }
}

std::tuple<Status, std::set<std::string>> Entry::propertiesNames() const {
    std::shared_lock locker{xLock_};

    try {
        return {Status::Ok(), record_.propertiesNames()};
    }
    catch (const std::bad_alloc&) { return {BadAllocThrownStatus, {}}; }
    catch (...) { return {ExceptionThrownStatus, {}}; }
}

Status Entry::expireProperty(const std::string &prop, chrono::milliseconds ms) {
    std::unique_lock locker{xLock_};

    try {
        auto status = record_.expireProperty(prop, ms);

        if (status.isOk())
            setDirty(true);

        return status;
    }
    catch (const std::bad_alloc&) { return BadAllocThrownStatus; }
    catch (...) { return ExceptionThrownStatus; }
}

Status Entry::cancelPropertyExpiration(const std::string &prop) {
    std::unique_lock locker{xLock_};

    try {
        auto status = record_.cancelPropertyExpiration(prop);

        if (status.isOk())
            setDirty(true);

        return status;
    }
    catch (const std::bad_alloc&) { return BadAllocThrownStatus; }
    catch (...) { return ExceptionThrownStatus; }
}

std::tuple<Status, std::set<std::string> > Entry::links() const {
    std::shared_lock locker{xLock_};

    try {
        auto children = record_.children();  // can throw here

        locker.unlock();

        std::set<std::string> ret;          // or here

        for (const auto& [name, handle] : children) {
            SKV_UNUSED(handle);

            ret.insert(name);               // or here
        }

        return {Status::Ok(), ret};
    }
    catch (const std::bad_alloc&) { return {BadAllocThrownStatus, {}}; }
    catch (...) { return {ExceptionThrownStatus, {}}; }
}

void Entry::setDirty(bool dirty) noexcept {
    dirty_ = dirty;
}

bool Entry::dirty() const noexcept {
    return dirty_;
}

Record &Entry::record() const noexcept {
    return record_;
}

std::shared_mutex &Entry::xLock() const noexcept {
    return xLock_;
}

}
