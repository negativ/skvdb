#include "Entry.hpp"
#include "util/ExceptionBoundary.hpp"

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

    Status ret;
    auto status = exceptionBoundary("ondisk::Entry::setProperty",
                                    [&] {
                                        ret = record_.setProperty(prop, value);

                                        if (ret.isOk())
                                            setDirty(true);
                                    });

    return status.isOk()? ret : status;
}

std::tuple<Status, Property> Entry::property(const std::string &prop) const {
    std::shared_lock locker{xLock_};

    std::tuple<Status, Property> ret;
    auto status = exceptionBoundary("ondisk::Entry::property",
                                    [&] {
                                        ret = record_.property(prop);
                                    });

    return status.isOk()? ret : std::make_tuple(status, Property{});
}

Status Entry::removeProperty(const std::string &prop) {
    std::unique_lock locker{xLock_};

    Status ret;
    auto status = exceptionBoundary("ondisk::Entry::removeProperty",
                                    [&] {
                                        ret = record_.removeProperty(prop);

                                        if (ret.isOk())
                                            setDirty(true);
                                    });

    return status.isOk()? ret : status;
}

std::tuple<Status, IEntry::Properties> Entry::properties() const {
    std::shared_lock locker{xLock_};

    std::tuple<Status, IEntry::Properties> ret;
    auto status = exceptionBoundary("ondisk::Entry::properties",
                                    [&] {
                                        ret = {Status::Ok(), record_.properties()};
                                    });

    return status.isOk()? ret : std::make_tuple(status, IEntry::Properties{});
}

std::tuple<Status, std::set<std::string>> Entry::propertiesNames() const {
    std::shared_lock locker{xLock_};

    std::tuple<Status, std::set<std::string>> ret;
    auto status = exceptionBoundary("ondisk::Entry::propertiesNames",
                                    [&] {
                                        ret = {Status::Ok(), record_.propertiesNames()};
                                    });

    return status.isOk()? ret : std::make_tuple(status, std::set<std::string>{});
}

Status Entry::expireProperty(const std::string &prop, chrono::milliseconds ms) {
    std::unique_lock locker{xLock_};

    Status ret;
    auto status = exceptionBoundary("ondisk::Entry::expireProperty",
                                    [&] {
                                        ret = record_.expireProperty(prop, ms);

                                        if (ret.isOk())
                                            setDirty(true);
                                    });

    return status.isOk()? ret : status;
}

Status Entry::cancelPropertyExpiration(const std::string &prop) {
    std::unique_lock locker{xLock_};

    Status ret;
    auto status = exceptionBoundary("ondisk::Entry::cancelPropertyExpiration",
                                    [&] {
                                        ret = record_.cancelPropertyExpiration(prop);

                                        if (ret.isOk())
                                            setDirty(true);
                                    });

    return status.isOk()? ret : status;
}

std::tuple<Status, std::set<std::string> > Entry::links() const {
    std::shared_lock locker{xLock_};

    std::tuple<Status, std::set<std::string>> ret;
    auto status = exceptionBoundary("ondisk::Entry::links",
                                    [&] {
                                        auto children = record_.children();

                                        locker.unlock();

                                        std::set<std::string> cs;

                                        for (const auto& [name, handle] : children) {
                                            SKV_UNUSED(handle);

                                            cs.insert(name);
                                        }

                                        ret = {Status::Ok(), cs};
                                    });

    return status.isOk()? ret : std::make_tuple(status, std::set<std::string>{});
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
