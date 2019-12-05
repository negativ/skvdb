#include "VirtualEntry.hpp"

namespace skv::vfs {

VirtualEntry::VirtualEntry(IEntry::Handle handle, VirtualEntry::Entries &&entries, VirtualEntry::Volumes &&volumes, ThreadPool &threadPool):
    handle_{handle},
    entries_{entries},
    volumes_{volumes},
    threadPool_{std::ref(threadPool)}
{

}

IEntry::Handle VirtualEntry::handle() const noexcept {
    return handle_;
}

std::string VirtualEntry::name() const {
    return ""; // TODO: implement
}

bool VirtualEntry::hasProperty(const std::string &prop) const noexcept {
    auto results = forEachEntry(&IEntry::hasProperty, prop);

    return std::any_of(std::cbegin(results), std::cend(results),
                       [](auto v) { return v; });
}

Status VirtualEntry::setProperty(const std::string &prop, const Property &value) {
    auto results = forEachEntry(&IEntry::setProperty, prop, value);

    auto ok = std::all_of(std::cbegin(results), std::cend(results),
                          [](auto&& status) { return status.isOk(); });

    return ok? Status::Ok() : Status::InvalidOperation("Unknown error");
}

std::tuple<Status, Property> VirtualEntry::property(const std::string &prop) const {
    auto results = forEachEntry(&IEntry::property, prop);

    for (const auto& [status, value] : results) {
        if (status.isOk())
            return {status, value};
    }

    return {Status::InvalidArgument("No such property"), {}};
}

Status VirtualEntry::removeProperty(const std::string &prop) {
    auto results = forEachEntry(&IEntry::removeProperty, prop);

    auto ok = std::any_of(std::cbegin(results), std::cend(results),
                          [](auto&& status) { return status.isOk(); });

    return ok? Status::Ok() : Status::InvalidArgument("No such property");
}

IEntry::Properties VirtualEntry::properties() const {
    auto results = forEachEntry(&IEntry::properties);

    if (results.empty())
        return {};

    Properties ret = std::move(results[0]); // just moving first result to result (properties from entry with highest priority)

    for (std::size_t i = 1; i < results.size(); ++i) {
        for (const auto& p : results[i])
            ret.insert(p);
    }

    return ret;
}

std::set<std::string> VirtualEntry::propertiesNames() const {
    auto results = forEachEntry(&IEntry::propertiesNames);

    if (results.empty())
        return {};

    std::set<std::string> ret = std::move(results[0]); // just moving first result to result (properties from entry with highest priority)

    for (std::size_t i = 1; i < results.size(); ++i) {
        for (auto& name : results[i])
            ret.insert(name);
    }

    return ret;
}

Status VirtualEntry::expireProperty(const std::string &prop, chrono::milliseconds ms) {
    auto results = forEachEntry(&IEntry::expireProperty, prop, ms);

    auto ok = std::any_of(std::cbegin(results), std::cend(results),
                          [](auto&& status) { return status.isOk(); });

    return ok? Status::Ok() : Status::InvalidArgument("No such property");
}

Status VirtualEntry::cancelPropertyExpiration(const std::string &prop) {
    auto results = forEachEntry(&IEntry::cancelPropertyExpiration, prop);

    auto ok = std::any_of(std::cbegin(results), std::cend(results),
                          [](auto&& status) { return status.isOk(); });

    return ok? Status::Ok() : Status::InvalidArgument("No such property");
}

std::set<std::string> VirtualEntry::children() const {
    auto results = forEachEntry(&IEntry::children);

    if (results.empty())
        return {};

    std::set<std::string> ret = std::move(results[0]); // just moving first result to result (properties from entry with highest priority)

    for (std::size_t i = 1; i < results.size(); ++i) {
        for (auto& child : results[i])
            ret.insert(child);
    }

    return ret;
}

VirtualEntry::Volumes &VirtualEntry::volumes() const noexcept {
    return volumes_;
}

VirtualEntry::Entries &VirtualEntry::entries() const noexcept {
    return entries_;
}



}
