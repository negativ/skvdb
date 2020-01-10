#include "VirtualEntry.hpp"
#include "util/ExceptionBoundary.hpp"

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

std::tuple<Status, bool> VirtualEntry::hasProperty(const std::string &prop) const noexcept {
    const auto& [status, results] = forEachEntry(&IEntry::hasProperty, prop);

    if (!status.isOk())
        return {status, false};

    return {Status::Ok(), std::any_of(std::cbegin(results), std::cend(results),
                                      [](auto result) {
                                            [[maybe_unused]] const auto& [status, v] = result;
                                            return v;
                                      })};
}

Status VirtualEntry::setProperty(const std::string &prop, const Property &value) {
    auto [status, results] = forEachEntry(&IEntry::setProperty, prop, value);

    if (!status.isOk())
        return status;

    auto ok = std::all_of(std::cbegin(results), std::cend(results),
                          [](auto&& status) { return status.isOk(); });

    return ok? Status::Ok() : Status::InvalidOperation("Unknown error");
}

std::tuple<Status, Property> VirtualEntry::property(const std::string &prop) const {
    const auto& [st, results] = forEachEntry(&IEntry::property, prop);

    if (!st.isOk())
        return {st, {}};

    for (const auto& [status, value] : results) {
        if (status.isOk())
            return {status, value};
    }

    return {Status::InvalidArgument("No such property"), {}};
}

Status VirtualEntry::removeProperty(const std::string &prop) {
    const auto& [status, results] = forEachEntry(&IEntry::removeProperty, prop);

    if (!status.isOk())
        return status;

    auto ok = std::any_of(std::cbegin(results), std::cend(results),
                          [](auto&& status) { return status.isOk(); });

    return ok? Status::Ok() : Status::InvalidArgument("No such property");
}

std::tuple<Status, IEntry::Properties> VirtualEntry::properties() const {
    Status status;
    std::vector<std::tuple<Status, IEntry::Properties>> results;

    std::tie(status, results) = forEachEntry(&IEntry::properties);

    if (!status.isOk())
        return {status, {}};

    if (results.empty())
        return {Status::Ok(), {}};

    std::tuple<Status, IEntry::Properties> ret;
    status = exceptionBoundary("VirtualEntry::properties",
                               [&] {
                                   Properties props;

                                   for (const auto& [st, ps] : results) {
                                       if (!st.isOk())
                                           continue;

                                       for (const auto& p : ps)
                                           props.insert(p);
                                   }

                                   ret = {Status::Ok(), props};
                               });

    return status.isOk()? ret : std::make_tuple(status, IEntry::Properties{});
}

std::tuple<Status, std::set<std::string>> VirtualEntry::propertiesNames() const {
    Status status;
    std::vector<std::tuple<Status, std::set<std::string>>> results;

    std::tie(status, results) = forEachEntry(&IEntry::propertiesNames);

    if (!status.isOk())
        return {status, {}};

    if (results.empty())
        return {Status::Ok(), {}};

    std::tuple<Status, std::set<std::string>> ret;
    status = exceptionBoundary("VirtualEntry::propertiesNames",
                               [&] {
                                   std::set<std::string> pns;

                                   for (const auto& [st, ps] : results) {
                                       if (!st.isOk())
                                           continue;

                                       for (const auto& p : ps)
                                           pns.insert(p);
                                   }

                                   ret = {Status::Ok(), pns};
                               });

    return status.isOk()? ret : std::make_tuple(status, std::set<std::string>{});
}

Status VirtualEntry::expireProperty(const std::string &prop, chrono::milliseconds ms) {
    const auto& [status, results] = forEachEntry(&IEntry::expireProperty, prop, ms);

    if (!status.isOk())
        return status;

    auto ok = std::any_of(std::cbegin(results), std::cend(results),
                          [](auto&& status) { return status.isOk(); });

    return ok? Status::Ok() : Status::InvalidArgument("No such property");
}

Status VirtualEntry::cancelPropertyExpiration(const std::string &prop) {
    const auto& [status, results] = forEachEntry(&IEntry::cancelPropertyExpiration, prop);

    if (!status.isOk())
        return status;

    auto ok = std::any_of(std::cbegin(results), std::cend(results),
                          [](auto&& status) { return status.isOk(); });

    return ok? Status::Ok() : Status::InvalidArgument("No such property");
}

std::tuple<Status, std::set<std::string>> VirtualEntry::links() const {
    Status status;
    std::vector<std::tuple<Status, std::set<std::string>>> results;

    std::tie(status, results) = forEachEntry(&IEntry::IEntry::links);

    if (!status.isOk())
        return {status, {}};

    if (results.empty())
        return {};

    std::tuple<Status, std::set<std::string>> ret;
    status = exceptionBoundary("VirtualEntry::links",
                               [&] {
                                   std::set<std::string> c;

                                   for (const auto& [st, cs] : results) {
                                       if (!st.isOk())
                                           continue;

                                       for (const auto& child : cs)
                                           c.insert(child);
                                   }

                                   ret = {Status::Ok(), c};
                               });

    return status.isOk()? ret : std::make_tuple(status, std::set<std::string>{});
}

VirtualEntry::Volumes &VirtualEntry::volumes() const noexcept {
    return volumes_;
}

VirtualEntry::Entries &VirtualEntry::entries() const noexcept {
    return entries_;
}

}
