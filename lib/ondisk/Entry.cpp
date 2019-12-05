#include "Entry.hpp"

namespace skv::ondisk {

Entry::Entry(Record &&record):
    record_{record}
{

}

IEntry::Handle Entry::handle() const noexcept {
    std::shared_lock locker{xLock_};

    return record_.handle();
}

bool Entry::hasProperty(const std::string &prop) const noexcept {
    std::shared_lock locker(xLock_);

    return record_.hasProperty(prop);
}

Status Entry::setProperty(const std::string &prop, const Property &value) {
    std::unique_lock locker{xLock_};

    auto status = record_.setProperty(prop, value);

    if (status.isOk())
        setDirty(true);

    return status;
}

std::tuple<Status, Property> Entry::property(const std::string &prop) const {
    std::shared_lock locker{xLock_};

    return record_.property(prop);
}

Status Entry::removeProperty(const std::string &prop) {
    std::unique_lock locker{xLock_};

    auto status = record_.removeProperty(prop);

    if (status.isOk())
        setDirty(true);

    return status;
}

IEntry::Properties Entry::properties() const {
    std::shared_lock locker{xLock_};

    return record_.properties();
}

std::set<std::string> Entry::propertiesNames() const {
    std::shared_lock locker{xLock_};

    return record_.propertiesNames();
}

Status Entry::expireProperty(const std::string &prop, chrono::milliseconds ms) {
    std::unique_lock locker{xLock_};

    auto status = record_.expireProperty(prop, ms);

    if (status.isOk())
        setDirty(true);

    return status;
}

Status Entry::cancelPropertyExpiration(const std::string &prop) {
    std::unique_lock locker{xLock_};

    auto status = record_.cancelPropertyExpiration(prop);

    if (status.isOk())
        setDirty(true);

    return status;
}

std::set<std::string> Entry::children() const {
    std::shared_lock locker{xLock_};

    auto children = record_.children();

    locker.unlock();

    std::set<std::string> ret;

    for (const auto& [name, handle] : children) {
        SKV_UNUSED(handle);

        ret.insert(name);
    }

    return ret;
}

void Entry::setDirty(bool dirty) {
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
