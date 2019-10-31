#pragma once

#include <cassert>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <set>
#include <string>
#include <type_traits>

#include <boost/endian/conversion.hpp>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/tag.hpp>

#include "Property.hpp"
#include "util/Status.hpp"
#include "util/Serialization.hpp"

namespace skv::ondisk {

namespace bmi = boost::multi_index;
namespace chrono = std::chrono;

using namespace skv::util;

template <typename Key,
          std::decay_t<Key> TInvalidKey = 0,
          typename PropertyName = std::string,
          typename PropertyValue = Property,
          typename PropertyContainer = std::map<std::decay_t<PropertyName>, std::decay_t<PropertyValue>>, // maybe btree_map is better choice
          typename ClockType = chrono::system_clock>
class Entry final {
public:
    using key_type              = std::decay_t<Key>;
    using prop_container_type   = std::decay_t<PropertyContainer>;
    using prop_name_type        = std::decay_t<typename prop_container_type::key_type>;
    using prop_value_type       = std::decay_t<PropertyValue>;
    using child_type            = std::pair<prop_name_type, key_type>;
    using children_type         = std::set<child_type>;
    using clock_type            = std::decay_t<ClockType>;

    static_assert (std::is_integral_v<key_type>, "Entry key should be an integral type");

    static constexpr key_t InvalidKey = TInvalidKey;

    Entry():
        impl_{std::make_unique<Impl>()}
    {

    }

    ~Entry() noexcept = default;

    Entry(key_type key, prop_name_type name):
        Entry()
    {
        impl_->key_ = key;
        impl_->name_ = name;
    }

    Entry(const Entry& other) {
        using std::swap;

        ImplPtr copy = std::make_unique<Impl>();
        *copy = *other.impl_;

        swap(impl_, copy);
    }

    Entry& operator=(const Entry& other) {
        using std::swap;

        ImplPtr copy = std::make_unique<Impl>();
        *copy = *other.impl_;

        swap(impl_, copy);

        return *this;
    }

    Entry(Entry&& other) noexcept {
        using std::swap;

        swap(impl_, other.impl_);
    }

    Entry& operator=(Entry&& other) noexcept {
        using std::swap;

        swap(impl_, other.impl_);

        return *this;
    }

    [[nodiscard]] key_type key() const noexcept {
        return impl_->key_;
    }

    [[nodiscard]] key_type parent() const noexcept {
        return impl_->parent_;
    }

    [[nodiscard]] prop_name_type name() const noexcept(std::is_nothrow_copy_constructible_v<prop_name_type>) {
        return impl_->name_;
    }

    [[nodiscard]] bool hasProperty(const prop_name_type& prop) const noexcept {
        if (propertyExpired(prop))
            return false;

        return impl_->properties_.find(prop) != std::cend(impl_->properties_);
    }

    Status setProperty(const prop_name_type& prop, const prop_value_type& value) {
        if (propertyExpired(prop))
            static_cast<void>(cancelPropertyExpiration(prop).isOk()); // undo expiration

        impl_->properties_[prop] = value;

        return Status::Ok();
    }

    [[nodiscard]] std::tuple<Status, prop_value_type> property(const prop_name_type& prop) const {
        if (propertyExpired(prop))
            return {Status::InvalidArgument("No such property"), {}};

        auto it = impl_->properties_.find(prop);

        if (it != std::cend(impl_->properties_))
            return {Status::Ok(), it->second};

        return {Status::InvalidArgument("No such property"), {}};
    }

    [[nodiscard]] Status removeProperty(const prop_name_type& prop) {
        static_cast<void>(cancelPropertyExpiration(prop).isOk());

        if  (impl_->properties_.erase(prop) > 0)
            return Status::Ok();

        return Status::InvalidArgument("No such property");
    }

    [[nodiscard]] Status expireProperty(const prop_name_type& prop, typename clock_type::time_point tp) {
        if (!hasProperty(prop))
            return Status::InvalidArgument("No such property");

        auto nowms = chrono::duration_cast<chrono::milliseconds>(clock_type::now().time_since_epoch()).count();
        auto expirationms = chrono::duration_cast<chrono::milliseconds>(tp.time_since_epoch()).count();

        if (nowms >= expirationms)
            return Status::InvalidArgument("Invalid timepoint");

        impl_->propertyExpireMap_[prop] = expirationms;

        return Status::Ok();
    }

    [[nodiscard]] Status cancelPropertyExpiration(const prop_name_type& prop) {
        impl_->propertyExpireMap_.erase(prop);

        return  Status::Ok();
    }

    [[nodiscard]] std::set<prop_name_type> propertiesSet() const {
        std::set<prop_name_type> ret;

        for (const auto& [prop, value] : impl_->properties_) { // rewritten to cycle after profiling
            if (!propertyExpired(prop))
                ret.insert(prop);
        }

        return ret;
    }

    [[nodiscard]] Status addChild(Entry& e) {
        if (e.parent() != InvalidKey)
            return Status::InvalidArgument("Entry already has a parent");

        child_type c{e.name(), e.key()};

        if (impl_->children_.insert(c).second) {
            e.setParent(key());

            return Status::Ok();
        }

        return Status::InvalidArgument("Duplicate entry");
    }

    [[nodiscard]] Status removeChild(Entry& e) {
        if (e.parent() != key())
            return Status::InvalidArgument("Not a child item");

        auto& index = impl_->children_.template get<typename Impl::ChildByKey>();
        auto it = index.find(e.key());

        if (it == std::end(index))
            return Status::InvalidArgument("No such child entry");

        index.erase(it);
        e.setParent(InvalidKey);

        return Status::Ok();
    }

    [[nodiscard]] children_type children() const {
        auto& index = impl_->children_.template get<typename Impl::ChildByKey>();
        children_type ret;

        std::transform(std::cbegin(index), std::cend(index),
                       std::inserter(ret, std::begin(ret)),
                       [](auto&& p) { return p; } );

        return ret;
    }

    [[nodiscard]] bool operator!=(const Entry& other) const noexcept {
        const_cast<Entry&>(*this).doPropertyCleanup();  // ¯\_(ツ)_/¯
        const_cast<Entry&>(other).doPropertyCleanup();  //

        return (*impl_ != *other.impl_);
    }

    [[nodiscard]] bool operator==(const Entry& other) const noexcept {
        const_cast<Entry&>(*this).doPropertyCleanup();  // ¯\_(ツ)_/¯
        const_cast<Entry&>(other).doPropertyCleanup();  //

        return (*impl_ == *other.impl_);
    }

    [[nodiscard]] bool operator<(const Entry& other) const noexcept {
        return (*impl_ < *other.impl_);
    }

    [[nodiscard]] bool operator>(const Entry& other) const noexcept {
        return (*impl_ > *other.impl_);
    }

private:
    template <typename K, K IK, typename PN, typename PV, typename PC>
    friend std::istream& operator>>(std::istream& _is, Entry<K, IK, PN, PV, PC> & p) ;

    template <typename K, K IK, typename PN, typename PV, typename PC>
    friend std::ostream& operator<<(std::ostream& _os, const Entry<K, IK, PN, PV, PC> & p) ;

    void setParent(key_type p) noexcept {
        impl_->parent_ = p;
    }

    /* Removing all expired properties */
    void doPropertyCleanup() {
        auto expired = std::accumulate(std::cbegin(impl_->propertyExpireMap_), std::cend(impl_->propertyExpireMap_),
                                       std::set<prop_name_type>{},
                                       [this](auto&& acc, auto&& p) {
                                            if (propertyExpired(p.first))
                                                acc.insert(p.first);
                                            return acc;
                                       });

        std::for_each(std::cbegin(expired), std::cend(expired),
                      [this](auto&& prop) { static_cast<void>(removeProperty(prop).isOk()); });
    }

    [[nodiscard]] bool propertyExpired(const prop_name_type& prop) const noexcept {
        auto it = impl_->propertyExpireMap_.find(prop);

        if (it == std::cend(impl_->propertyExpireMap_))
            return false;

        auto now = chrono::duration_cast<chrono::milliseconds>(clock_type::now().time_since_epoch()).count();
        auto exp = it->second;

        return (now >= exp);
    }

    struct Impl {
        struct ChildByName{}; struct ChildByKey {};

        using child_container = boost::multi_index_container<child_type,
                                                             bmi::indexed_by<
                                                                bmi::ordered_unique<bmi::tag<ChildByName>,
                                                                                    bmi::member<child_type, typename child_type::first_type,  &child_type::first>>,
                                                                bmi::ordered_unique<bmi::tag<ChildByKey>,
                                                                                    bmi::member<child_type, typename child_type::second_type, &child_type::second>>>>;

        key_type key_;
        key_type parent_{InvalidKey};
        prop_container_type properties_;
        prop_name_type name_;
        child_container children_;
        std::map<prop_name_type, std::int64_t> propertyExpireMap_;

        bool operator==(const Impl& other) const noexcept {
            return key_ == other.key_ &&
                   parent_ == other.parent_ &&
                   properties_ == other.properties_ &&
                   name_ == other.name_ &&
                   children_ == other.children_ &&
                   propertyExpireMap_ == other.propertyExpireMap_;
        }

        bool operator!=(const Impl& other) const noexcept {
            return !(*this == other);
        }

        bool operator<(const Impl& other) const noexcept {
            return key_ < other.key_;
        }

        bool operator>(const Impl& other) const noexcept {
            return key_ > other.key_;
        }
    };

    using ImplPtr = std::unique_ptr<Impl>;

    ImplPtr impl_;
};

template <typename Key,
          Key TInvalidKey,
          typename PropertyName,
          typename PropertyValue,
          typename PropertyContainer>
inline std::istream& operator>>(std::istream& _is, Entry<Key, TInvalidKey, PropertyName, PropertyValue, PropertyContainer> & p) {
    namespace be = boost::endian;

    using E = Entry<Key, TInvalidKey, PropertyName, PropertyValue, PropertyContainer>;

    Deserializer ds{_is};

    decltype (p.key()) key;
    decltype (p.parent()) parent;
    decltype (p.name()) name;

    ds >> key
       >> parent
       >> name;

    E ret = E{key, name};
    ret.setParent(parent);

    std::uint64_t propertiesCount;
    ds >> propertiesCount;

    for (decltype (propertiesCount) i = 0; i < propertiesCount; ++i) {
        typename E::prop_name_type prop;
        typename E::prop_value_type value;

        ds >> prop
           >> value;

        [[maybe_unused]] auto r = ret.setProperty(prop, value);
        assert(r.isOk());
    }

    std::uint64_t childrenCount;
    ds >> childrenCount;

    for (decltype (childrenCount) i = 0; i < childrenCount; ++i) {
        typename E::child_type::first_type cname;
        typename E::child_type::second_type ckey;

        ds >> cname
           >> ckey;

        E child(ckey, cname);

        [[maybe_unused]] auto status = ret.addChild(child);

        assert(status.isOk());
    }

    std::uint64_t expirePropertyCount;
    ds >> expirePropertyCount;

    auto& propertyExpire = ret.impl_->propertyExpireMap_;

    for (decltype (expirePropertyCount) i = 0; i < expirePropertyCount; ++i) {
        typename E::prop_name_type pname;
        std::int64_t ts;

        ds >> pname
           >> ts;

        propertyExpire[pname] = ts;
    }

    ret.doPropertyCleanup();

    p = std::move(ret);

    return _is;
}

template <typename Key,
          Key TInvalidKey,
          typename PropertyName,
          typename PropertyValue,
          typename PropertyContainer>
inline std::ostream& operator<<(std::ostream& _os, const Entry<Key, TInvalidKey, PropertyName, PropertyValue, PropertyContainer> & p) {
    namespace be = boost::endian;
    using E = Entry<Key, TInvalidKey, PropertyName, PropertyValue, PropertyContainer>;

    const_cast<E&>(p).doPropertyCleanup();

    Serializer s{_os};

    s << p.key()
      << p.parent()
      << p.name();

    const auto& properties = p.impl_->properties_;
    std::uint64_t propertiesCount = properties.size();

    s << propertiesCount;

    for (const auto& [prop, value] : properties) {
        s << prop
          << value;
    }

    auto children = p.children();
    std::uint64_t childrenCount = children.size();

    s << childrenCount;

    for (const auto& c : children) {
        const auto& [name, key] = c;

        s << name
          << key;
    }

    auto propertyExpire = p.impl_->propertyExpireMap_;
    std::uint64_t expirePropertyCount = propertyExpire.size();

    s << expirePropertyCount;

    for (const auto& c : propertyExpire) {
        const auto& [name, key] = c;

        s << name
          << key;
    }

    return _os;
}

}
