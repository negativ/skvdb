#include "Storage.hpp"

#include <algorithm>
#include <future>
#include <thread>
#include <type_traits>
#include <unordered_map>

#include "MountPoint.hpp"
#include "VirtualEntry.hpp"
#include "util/Log.hpp"
#include "util/String.hpp"
#include "util/StringPath.hpp"
#include "util/StringPathIterator.hpp"
#include "util/Unused.hpp"

namespace {

using namespace skv::util;

const Status InvalidVolumeArgumentStatus = Status::InvalidArgument("Invalid volume");

}

namespace skv::vfs {

using namespace skv::util;

template <class IIt, class OIt, class UnaryOp, class Pred>
OIt transform_future_if(IIt start, IIt stop, OIt result, Pred pred, UnaryOp op) {
    while (start != stop) {
        auto&& ret = (*start).get();

        if (pred(ret)) {
            *result = std::move(op(std::move(ret)));
            ++result;
        }
        ++start;
    }

    return result;
}

template <class IIt, class OIt, class UnaryOp>
OIt transform_future(IIt start, IIt stop, OIt result, UnaryOp op) {
    while (start != stop) {
        *result = std::move(op(std::move((*start).get())));
        ++result;
        ++start;
    }

    return result;
}

using VirtualEntries = std::vector<VirtualEntry>;

struct Storage::Impl {
    std::tuple<Status, Storage::Handle> open(std::string_view path) {
        using result_t = std::tuple<Status, VirtualEntry>;
        using future_result_t = std::future<result_t>;

        std::string vpath = simplifyPath(path);
        auto [status, mountPath] = searchMountPathFor(vpath);

        if (!status.isOk())
            return {status, Storage::InvalidHandle};

        auto& index = mpoints_.get<mount::tags::ByMountPath>();
        auto [start, stop] = index.equal_range(util::to_string(mountPath));
        auto subvpath = vpath.substr(mountPath.size(), vpath.size() - mountPath.size()); // extracting subpath from vpath
        std::vector<future_result_t> results;

        std::transform(start, stop,
                       std::back_inserter(results),
                       [subvpath](auto&& entry) {
                           return std::async(std::launch::async, [=]() -> result_t {
                               auto volume  = entry.volume();
                               auto subpath = simplifyPath(entry.entryPath() + "/" + subvpath);
                               auto [status, handle] = volume->open(subpath);

                               if (status.isOk())
                                   return {status, VirtualEntry(subpath, volume, handle, entry.priority())};

                               return {Status::InvalidArgument("Invalid volume"), VirtualEntry{}};
                           });
                       });

        VirtualEntries openedEntries;

        transform_future_if(std::begin(results), std::end(results),
                            std::back_inserter(openedEntries),
                            [](const auto& t) {
                                const auto& [status, unused] = t;
                                return status.isOk();
                            },
                            [](auto&& t) {
                                auto&& [unused, ventry] = t;
                                return ventry;
                            } );

        if (openedEntries.empty())
            return {Status::InvalidArgument("No such path"), Storage::InvalidHandle};

        return {Status::Ok(), addVirtualEntries(std::move(openedEntries))};
    }

    Status close(Storage::Handle handle) {
        using result_t = Status;
        using future_result_t = std::future<result_t>;

        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return status;

        std::vector<future_result_t> results;
        std::transform(std::begin(ventries), std::end(ventries),
                       std::back_inserter(results),
                       [&](auto&& entry) {
                           return std::async(std::launch::async, [=]() -> result_t {
                               auto volume = entry.volume();

                               if (auto volume = entry.volume().lock(); volume)
                                   return volume->close(entry.handle());

                               return Status::InvalidArgument("Invalid volume");
                           });
                       });

        std::vector<bool> statuses;
        transform_future(std::begin(results), std::end(results),
                         std::back_inserter(statuses),
                         [](auto&& status) { return status.isOk(); });

        removeVirtualEntries(handle);

        auto ret = std::none_of(std::cbegin(statuses), std::cend(statuses), std::logical_not<bool>());

        return ret? Status::Ok() : Status::InvalidOperation("Unable to close handle of all volumes");
    }

    std::tuple<Status, Storage::Properties> properties(Storage::Handle handle) {
        using result_t = std::tuple<Status, Storage::Properties>;
        using future_result_t = std::future<result_t>;

        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return {status, {}};

        std::vector<future_result_t> futresults;
        std::transform(std::begin(ventries), std::end(ventries),
                       std::back_inserter(futresults),
                       [&](auto&& entry) {
                           return std::async(std::launch::async, [=]() -> result_t {
                               auto volume = entry.volume();

                               if (auto volume = entry.volume().lock(); volume)
                                   return volume->properties(entry.handle());

                               return {Status::InvalidArgument("Invalid volume"), {}};
                           });
                       });

        std::vector<result_t> results;
        transform_future_if(std::begin(futresults), std::end(futresults),
                            std::back_inserter(results),
                            [](const auto& t) {
                                const auto& [status, unused] = t;
                                return status.isOk();
                            },
                            [](auto&& res) { return std::move(res); });

        if (results.size() != futresults.size())
            return {Status::InvalidArgument("Unable to fetch properties from all volumes"), {}};

        Storage::Properties ret;

        for (auto&& [status, props] : results) {
            for (auto&& [prop, value] : props) {
                if (auto it = ret.find(prop); it == std::cend(ret))
                    ret.emplace(std::move(prop), std::move(value));
            }
        }

        return {Status::Ok(), ret};
    }

    std::tuple<Status, Property> property(Storage::Handle handle, std::string_view name) {
        using result_t = std::tuple<Status, Property>;
        using future_result_t = std::future<result_t>;

        const auto& [status, ventries] = getVirtualEntries(handle);

        if (!status.isOk())
            return {status, {}};

        std::vector<future_result_t> futresults;
        std::transform(std::begin(ventries), std::end(ventries),
                       std::back_inserter(futresults),
                       [name](auto&& entry) {
                           return std::async(std::launch::async, [=]() -> result_t {
                               auto volume = entry.volume();

                               if (auto volume = entry.volume().lock(); volume)
                                   return volume->property(entry.handle(), name);

                               return {Status::InvalidArgument("Invalid volume"), {}};
                           });
                       });

        std::vector<result_t> results;
        transform_future_if(std::begin(futresults), std::end(futresults),
            std::back_inserter(results),
            [](const auto& t) {
                const auto& [status, unused] = t;
                return status.isOk();
            },
            [](auto&& res) { return std::move(res); });

        if (results.empty())
            return {Status::InvalidArgument("No such property"), {}};

        auto [unused, value] = results.front(); // property from volume with highest priority

        return {Status::Ok(), value};
    }

    Status setProperty(Storage::Handle handle, std::string_view name, const Property &value) {
        return Status::Ok();
    }

    Status removeProperty(Storage::Handle handle, std::string_view name) {
        return Status::Ok();
    }

    std::tuple<Status, Storage::Links> links(Storage::Handle handle) {
        return {Status::Ok(), {}};
    }

    std::tuple<Status, bool> hasProperty(Storage::Handle handle, std::string_view name) {
        return {Status::Ok(), {}};
    }

    std::tuple<Status, std::string> searchMountPathFor(std::string_view path) const {
        auto searchPath = simplifyPath(path);
        ReverseStringPathIterator start{searchPath}, stop{};

        auto& index = mpoints_.get<mount::tags::ByMountPath>();

        while (start != stop) {
            auto it = index.find(*start);

            if (it != std::cend(index))
                return {Status::Ok(), it->mountPath()};

            ++start;
        }

        return {Status::NotFound("Unable to find mount point"), {}};
    }

    Storage::Handle addVirtualEntries(VirtualEntries&& ventries) {
        // TODO: lock
        auto handle = newHandle();

        std::sort(std::begin(ventries), std::end(ventries), std::greater<VirtualEntry>()); //sort from high priority to low

        if (ventries_.emplace(handle, ventries).second)
            return handle;

        return Storage::InvalidHandle;
    }

    std::tuple<Status, VirtualEntries> getVirtualEntries(Storage::Handle handle) const {
        // TODO: lock
        auto it = ventries_.find(handle);

        if (it == std::cend(ventries_))
            return {Status::InvalidArgument("No such handle"), {}};

        return {Status::Ok(), it->second};
    }

    void removeVirtualEntries(Storage::Handle handle) {
        // TODO: lock
        auto it = ventries_.find(handle);

        if (it != std::end(ventries_))
            ventries_.erase(it);
    }

    Storage::Handle newHandle() noexcept {
        return currentHandle_.fetch_add(1);
    }

    mount::Points mpoints_{};
    std::unordered_map<Storage::Handle, VirtualEntries> ventries_;
    std::atomic<Storage::Handle> currentHandle_{IVolume::RootHandle + 1};
};

Storage::Storage():
    impl_{new Impl{}}
{

}

Storage::~Storage() noexcept {

}

std::tuple<Status, Storage::Handle> Storage::open(std::string_view path) {
    return impl_->open(path);
}

Status Storage::close(Storage::Handle handle) {
    return impl_->close(handle);
}

std::tuple<Status, Storage::Links> Storage::links(Storage::Handle handle) {
    return impl_->links(handle);
}

std::tuple<Status, Storage::Properties > Storage::properties(Storage::Handle handle) {
    return impl_->properties(handle);
}

std::tuple<Status, Property> Storage::property(Storage::Handle handle, std::string_view name) {
    return impl_->property(handle, name);
}

Status Storage::setProperty(Storage::Handle handle, std::string_view name, const Property &value) {
    return impl_->setProperty(handle, name, value);
}

Status Storage::removeProperty(Storage::Handle handle, std::string_view name) {
    return impl_->removeProperty(handle, name);
}

std::tuple<Status, bool> Storage::hasProperty(Storage::Handle handle, std::string_view name) {
    return impl_->hasProperty(handle, name);
}

Status Storage::expireProperty(Storage::Handle handle, std::string_view name, chrono::system_clock::time_point tp) {

}

Status Storage::cancelPropertyExpiration(Storage::Handle handle, std::string_view name) {

}

Status Storage::link(Storage::Handle handle, std::string_view name) {

}

Status Storage::unlink(Handle handle, std::string_view name) {

}

Status Storage::mount(IVolumePtr volume, std::string_view entryPath, std::string_view mountPath, Storage::Priority prio) {
    if (!volume)
        return InvalidVolumeArgumentStatus;

    if (!volume->claim(this).isOk())
        return Status::InvalidOperation("Volume claimed by another instance of storage");

    mount::Entry entry(mountPath, entryPath, volume, prio);

    if (!entry.open()) {
        SKV_UNUSED(volume->release(this));

        return Status::InvalidArgument("Unable to create mount point entry. Check volume is properly initialized and entry path exists.");   
    }

    // TODO: protect by mutex

    auto& index = impl_->mpoints_.get<mount::tags::ByAll>();
    auto retp = index.insert(std::move(entry));

    if (!retp.second) {
        SKV_UNUSED(volume->release(this));
        entry.close();

        return Status::InvalidArgument("Mount point entry already exist");
    }

    return Status::Ok();
}

Status Storage::unmount(IVolumePtr volume, std::string_view entryPath, std::string_view mountPath) {
    if (!volume)
        return InvalidVolumeArgumentStatus;

    // TODO: protect by mutex

    auto& index = impl_->mpoints_.get<mount::tags::ByMountPath>();
    auto [start, stop] = index.equal_range(util::to_string(mountPath));
    auto entryIt = std::find_if(start, stop,
                                [&](auto&& entry) {
                                    return entry.volume() == volume && entry.entryPath() == entryPath;
                                });

    if (entryIt == std::end(index))
        return Status::InvalidArgument("No such mount point entry");

    if (!volume->release(this).isOk())
        return Status::Fatal("Unable to release volume");

    mount::Entry e(std::move(*entryIt));
    e.close();

    index.erase(entryIt);

    return Status::Ok();
}

}

