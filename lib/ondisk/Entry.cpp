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
    catch (...) {                           // Record::setProperty() can throw only std::bad_alloc
        return Status::Fatal("Exception");  // so it's not safe to call any function, just return
    }
}

std::tuple<Status, Property> Entry::property(const std::string &prop) const {
    std::shared_lock locker{xLock_};

    try {
        return record_.property(prop);
    }
    catch (...) {                                 // Record::property() can throw only std::bad_alloc
        return {Status::Fatal("Exception"), {}};  // so it's not safe to call any function, just return
    }
}

Status Entry::removeProperty(const std::string &prop) {
    std::unique_lock locker{xLock_};

    try {
        auto status = record_.removeProperty(prop);

        if (status.isOk())
            setDirty(true);

        return status;
    }
    catch (...) {                           // Record::removeProperty() can throw only std::bad_alloc
        return Status::Fatal("Exception");  // so it's not safe to call any function, just return
    }
}

std::tuple<Status, IEntry::Properties> Entry::properties() const {
    std::shared_lock locker{xLock_};

    try {
        return {Status::Ok(), record_.properties()};
    }
    catch (...) {                                 // Record::properties() can throw only std::bad_alloc
        return {Status::Fatal("Exception"), {}};  // so it's not safe to call any function, just return
    }
}

std::tuple<Status, std::set<std::string>> Entry::propertiesNames() const {
    std::shared_lock locker{xLock_};

    try {
        return {Status::Ok(), record_.propertiesNames()};
    }
    catch (...) {                                 // Record::propertiesNames() can throw only std::bad_alloc
        return {Status::Fatal("Exception"), {}};  // so it's not safe to call any function, just return
    }
}

Status Entry::expireProperty(const std::string &prop, chrono::milliseconds ms) {
    std::unique_lock locker{xLock_};

    try {
        auto status = record_.expireProperty(prop, ms);

        if (status.isOk())
            setDirty(true);

        return status;
    }
    catch (...) {                           // Record::expireProperty() can throw only std::bad_alloc
        return Status::Fatal("Exception");  // so it's not safe to call any function, just return
    }
}

Status Entry::cancelPropertyExpiration(const std::string &prop) {
    std::unique_lock locker{xLock_};

    try {
        auto status = record_.cancelPropertyExpiration(prop);

        if (status.isOk())
            setDirty(true);

        return status;
    }
    catch (...) {                           // Record::cancelPropertyExpiration() can throw only std::bad_alloc
        return Status::Fatal("Exception");  // so it's not safe to call any function, just return
    }
}

std::tuple<Status, std::set<std::string> > Entry::children() const {
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
    catch (...) {
        return {Status::Fatal("Exception"), {}};  // it's not safe to call any function, just return
    }
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
