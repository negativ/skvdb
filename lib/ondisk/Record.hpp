#pragma once

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <type_traits>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/tag.hpp>

#include "Property.hpp"
#include "vfs/IEntry.hpp"
#include "vfs/IVolume.hpp"
#include "util/Status.hpp"
#include "util/Serialization.hpp"
#include "util/Unused.hpp"

namespace skv::ondisk {

namespace bmi = boost::multi_index;
namespace chrono = std::chrono;

using namespace skv::util;
using namespace skv::vfs;

/**
 * @brief Volume entry
 */
class Record final {
public:
    using Child = std::pair<std::string, IEntry::Handle>;
    using Children = std::map<std::string, IEntry::Handle>;

    Record():
        impl_{std::make_unique<Impl>()}
    {

    }

    ~Record() noexcept  = default;

    Record(IEntry::Handle handle, std::string name):
        Record()
    {
        impl_->key_ = handle;
        impl_->name_ = std::move(name);
    }

    Record(const Record& other) {
        using std::swap;

        ImplPtr copy = std::make_unique<Impl>();
        *copy = *other.impl_;

        swap(impl_, copy);
    }

    Record& operator=(const Record& other) {
        using std::swap;

        ImplPtr copy = std::make_unique<Impl>();
        *copy = *other.impl_;

        swap(impl_, copy);

        return *this;
    }

    Record(Record&& other) noexcept {
        using std::swap;

        swap(impl_, other.impl_);
    }

    Record& operator=(Record&& other) noexcept {
        using std::swap;

        swap(impl_, other.impl_);

        return *this;
    }

    IEntry::Handle handle() const noexcept  {
        return impl_->key_;
    }

    IEntry::Handle parent() const noexcept {
        return impl_->parent_;
    }

    std::string name() const  {
        return impl_->name_;
    }

    bool hasProperty(const std::string& prop) const noexcept  {
        if (propertyExpired(prop))
            return false;

        return impl_->properties_.find(prop) != std::cend(impl_->properties_);
    }

    Status setProperty(const std::string& prop, const Property& value)  {
        cancelPropertyExpiration(prop); // undo expiration

        impl_->properties_[prop] = value;

        return Status::Ok();
    }

    std::tuple<Status, Property> property(const std::string& prop) const  {
        if (propertyExpired(prop))
            return {Status::NotFound("No such property"), {}};

        auto it = impl_->properties_.find(prop);

        if (it != std::cend(impl_->properties_))
            return {Status::Ok(), it->second};

        return {Status::NotFound("No such property"), {}};
    }

    Status removeProperty(const std::string& prop)  {
        cancelPropertyExpiration(prop);

        if (impl_->properties_.erase(prop) > 0)
            return Status::Ok();

        return Status::NotFound("No such property");
    }

    Status expireProperty(const std::string& prop, chrono::milliseconds tp)  {
        if (!hasProperty(prop))
            return Status::NotFound("No such property");

        auto nowms = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch());

        impl_->propertyExpireMap_[prop] = (nowms + tp).count();

        return Status::Ok();
    }

    Status cancelPropertyExpiration(const std::string& prop)  {
        impl_->propertyExpireMap_.erase(prop);

        return  Status::Ok();
    }

    IEntry::Properties properties() const  {
        IEntry::Properties ret;

        for (const auto& [prop, value] : impl_->properties_) {
            if (!propertyExpired(prop))
                ret[prop] = value;
        }

        return ret;
    }

    std::set<std::string> propertiesNames() const  {
        std::set<std::string> ret;

        for (const auto& [prop, value] : impl_->properties_) {
            SKV_UNUSED(value);
            
            if (!propertyExpired(prop))
                ret.insert(prop);
        }

        return ret;
    }

    Status addChild(Record& e) {
        if (e.parent() != IVolume::InvalidHandle)
            return Status::InvalidArgument("Entry already has a parent");

        Child c{e.name(), e.handle()};

        if (impl_->children_.insert(c).second) {
            e.setParent(handle());

            return Status::Ok();
        }

        return Status::InvalidArgument("Duplicate entry");
    }

    Status removeChild(Record& e) {
        auto& index = impl_->children_.template get<typename Impl::ChildByKey>();

        auto it = index.find(e.handle());

        if (it == std::end(index))
            return Status::InvalidArgument("No such child entry");

        index.erase(it);
        e.setParent(IVolume::InvalidHandle);

        return Status::Ok();
    }

    Children children() const {
        auto& index = impl_->children_.template get<typename Impl::ChildByKey>();
        Children ret;

        for (const auto& [name, handle] : index)
            ret[name] = handle;

        return ret;
    }

    [[nodiscard]] bool operator==(const Record& other) const noexcept {
        return handle() == other.handle() &&
               parent() == other.parent() &&
               name() == other.name() &&
               children() == other.children() &&
               properties() == other.properties(); // need better way to do this
    }

    [[nodiscard]] bool operator!=(const Record& other) const noexcept {
        return !(*this == other);
    }

    [[nodiscard]] bool operator<(const Record& other) const noexcept {
        return handle() < other.handle();
    }

    [[nodiscard]] bool operator>(const Record& other) const noexcept {
        return handle() > other.handle();
    }

private:
    friend std::ostream& operator<<(std::ostream& _os, const Record& p);
    friend std::istream& operator>>(std::istream& _is, Record& p);

    void setParent(IEntry::Handle p) noexcept {
        impl_->parent_ = p;
    }

    /* Removing all expired properties */
    void doPropertyCleanup() {
        std::set<std::string> expired;

        for (const auto& [prop, tp] : impl_->propertyExpireMap_) {
            SKV_UNUSED(tp);

            if (propertyExpired(prop))
                expired.insert(prop);
        }

        for (const auto& prop : expired)
            removeProperty(prop);
    }

    [[nodiscard]] bool propertyExpired(const std::string& prop) const noexcept {
        auto it = impl_->propertyExpireMap_.find(prop);

        if (it == std::cend(impl_->propertyExpireMap_))
            return false;

        auto now = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch()).count();
        auto exp = it->second;

        return (now >= exp);
    }

    struct Impl {
        struct ChildByName{}; struct ChildByKey {};


        using Children = boost::multi_index_container<Child,
                                                      bmi::indexed_by<
                                                        bmi::ordered_unique<bmi::tag<ChildByName>,
                                                                            bmi::member<Child, Child::first_type,  &Child::first>>,
                                                        bmi::ordered_unique<bmi::tag<ChildByKey>,
                                                                            bmi::member<Child, Child::second_type, &Child::second>>>>;

        IEntry::Handle key_{ IVolume::InvalidHandle };
        IEntry::Handle parent_{ IVolume::InvalidHandle };
        IEntry::Properties properties_;
        std::string name_;
        Impl::Children children_;
        std::map<std::string, std::int64_t> propertyExpireMap_;
    };

    using ImplPtr = std::unique_ptr<Impl>;

    ImplPtr impl_;


};

inline std::istream& operator>>(std::istream& _is, Record& p) {
    namespace be = boost::endian;

    Deserializer ds{_is};

    decltype (p.handle()) handle;
    decltype (p.parent()) parent;
    decltype (p.name()) name;

    ds >> handle
       >> parent
       >> name;

    Record ret{handle, name};
    ret.setParent(parent);

    std::uint64_t propertiesCount;
    ds >> propertiesCount;

    for (decltype (propertiesCount) i = 0; i < propertiesCount; ++i) {
        std::string prop;
        Property value;

        ds >> prop
           >> value;

        [[maybe_unused]] auto r = ret.setProperty(prop, value);
        assert(r.isOk());
    }

    std::uint64_t childrenCount;
    ds >> childrenCount;

    for (decltype (childrenCount) i = 0; i < childrenCount; ++i) {
        std::string cname;
        decltype (p.handle()) chandle;

        ds >> cname
           >> chandle;

        Record child(chandle, cname);

        [[maybe_unused]] auto status = ret.addChild(child);

        assert(status.isOk());
    }

    std::uint64_t expirePropertyCount;
    ds >> expirePropertyCount;

    auto& propertyExpire = ret.impl_->propertyExpireMap_;

    for (decltype (expirePropertyCount) i = 0; i < expirePropertyCount; ++i) {
        typename std::string pname;
        std::int64_t ts;

        ds >> pname
           >> ts;

        propertyExpire[pname] = ts;
    }

    ret.doPropertyCleanup();

    p = std::move(ret);

    return _is;
}

inline std::ostream& operator<<(std::ostream& _os, const Record& p) {
    const_cast<Record&>(p).doPropertyCleanup();

    Serializer s{_os};

    s << p.handle()
      << p.parent()
      << p.name();

    const auto& properties = p.impl_->properties_;
    std::uint64_t propertiesCount = properties.size();

    s << propertiesCount;

    for (const auto& [prop, value] : properties) {
        s << prop
          << value;
    }

    const auto& children = p.children();
    std::uint64_t childrenCount = children.size();

    s << childrenCount;

    for (const auto& c : children) {
        const auto& [name, handle] = c;

        s << name
          << handle;
    }

    const auto& propertyExpire = p.impl_->propertyExpireMap_;
    std::uint64_t expirePropertyCount = propertyExpire.size();

    s << expirePropertyCount;

    for (const auto& c : propertyExpire) {
        const auto& [name, tp] = c;

        s << name
          << tp;
    }

    return _os;
}

}
