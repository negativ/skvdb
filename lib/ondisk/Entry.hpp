#pragma once

#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
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

namespace skv::ondisk {

namespace bmi = boost::multi_index;
using namespace skv::util;

template <typename Key,
          std::decay_t<Key> TInvalidKey = 0,
          typename PropertyName = std::string,
          typename PropertyValue = Property,
          typename PropertyContainer = std::map<std::decay_t<PropertyName>, std::decay_t<PropertyValue>>> // maybe btree_map is better choice
class Entry final {
public:
    using key_type              = std::decay_t<Key>;
    using prop_container_type   = std::decay_t<PropertyContainer>;
    using prop_name_type        = std::decay_t<typename prop_container_type::key_type>;
    using prop_value_type       = std::decay_t<PropertyValue>;
    using child_type            = std::pair<prop_name_type, key_type>;

    static_assert (std::is_integral_v<key_type>, "Entry key should be an integral type");

    static constexpr key_t InvalidKey = TInvalidKey;

    Entry():
        impl_{std::make_unique<Impl>()}
    {

    }

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
        return impl_->properties_.find(prop) != std::cend(impl_->properties_);
    }

    bool setProperty(const prop_name_type& prop, const prop_value_type& value) {
        impl_->properties_[prop] = value;

        return true;
    }

    [[nodiscard]] std::tuple<Status, prop_value_type> property(const prop_name_type& prop) const {
        auto it = impl_->properties_.find(prop);

        if (it != std::cend(impl_->properties_))
            return {Status::Ok(), it->second};

        return {Status::InvalidArgument("No such property"), {}};
    }

    [[nodiscard]] bool removeProperty(const prop_name_type& prop) {
        return (impl_->properties_.erase(prop) > 0);
    }

    [[nodiscard]] std::set<prop_name_type> propertiesSet() const {
        std::set<prop_name_type> ret;

        std::transform(std::cbegin(impl_->properties_), std::cend(impl_->properties_),
                       std::inserter(ret, std::begin(ret)),
                       [](auto&& p) {
                            return p.first;
                       });

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

    [[nodiscard]] std::set<child_type> children() const {
        auto& index = impl_->children_.template get<typename Impl::ChildByKey>();
        std::set<child_type> ret;

        std::transform(std::cbegin(index), std::cend(index),
                       std::inserter(ret, std::begin(ret)),
                       [](auto&& p) { return p; } );

        return ret;
    }

    [[nodiscard]] bool operator!=(const Entry& other) const noexcept {
        return (*impl_ != *other.impl_);
    }

    [[nodiscard]] bool operator==(const Entry& other) const noexcept {
        return (*impl_ == *other.impl_);
    }

    [[nodiscard]] bool operator<(const Entry& other) const noexcept {
        return (*impl_ < *other.impl_);
    }

    [[nodiscard]] bool operator>(const Entry& other) const noexcept {
        return (*impl_ > *other.impl_);
    }

    void markAsDirty() noexcept {
        impl_->dirtyFlag_ = true;
    }

    void clearDirty() {
        impl_->dirtyFlag_ = false;
    }

private:
    template <typename K, K IK, typename PN, typename PV, typename PC>
    friend std::istream& operator>>(std::istream& is, Entry<K, IK, PN, PV, PC> & p) ;

    void setParent(key_type p) noexcept {
        impl_->parent_ = p;
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

        bool dirtyFlag_;

        bool operator==(const Impl& other) const noexcept {
            return key_ == other.key_ &&
                   parent_ == other.parent_ &&
                   properties_ == other.properties_ &&
                   name_ == other.name_ &&
                   children_ == other.children_ &&
                   dirtyFlag_ == other.dirtyFlag_;
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
inline std::istream& operator>>(std::istream& is, Entry<Key, TInvalidKey, PropertyName, PropertyValue, PropertyContainer> & p) {
    namespace be = boost::endian;

    using E = Entry<Key, TInvalidKey, PropertyName, PropertyValue, PropertyContainer>;

    decltype (p.key()) key;
    decltype (p.parent()) parent;
    std::uint64_t nameLength;

    is.read(reinterpret_cast<char*>(&key), sizeof(key));
    is.read(reinterpret_cast<char*>(&parent), sizeof(parent));
    is.read(reinterpret_cast<char*>(&nameLength), sizeof (nameLength));

    be::little_to_native_inplace(key);
    be::little_to_native_inplace(parent);
    be::little_to_native_inplace(nameLength);

    decltype (p.name()) name(nameLength, '\0');

    is.read(name.data(), nameLength);

    E ret = E{key, name};
    ret.setParent(parent);

    std::uint64_t propertiesCount;
    is.read(reinterpret_cast<char*>(&propertiesCount), sizeof (propertiesCount));

    be::little_to_native_inplace(propertiesCount);

    for (decltype (propertiesCount) i = 0; i < propertiesCount; ++i) {
        decltype(nameLength) pLen;

        is.read(reinterpret_cast<char*>(&pLen), sizeof(pLen));
        be::little_to_native_inplace(pLen);

        typename E::prop_name_type pname(pLen, '\0');

        is.read(pname.data(), std::streamsize(pLen));

        typename E::prop_value_type pval;

        is >> pval;

        assert(ret.setProperty(pname, pval));
    }

    std::uint64_t childrenCount;
    is.read(reinterpret_cast<char*>(&childrenCount), sizeof (childrenCount));
    be::little_to_native_inplace(childrenCount);

    for (decltype (childrenCount) i = 0; i < childrenCount; ++i) {
        decltype(nameLength) pLen;

        is.read(reinterpret_cast<char*>(&pLen), sizeof(pLen));
        be::little_to_native_inplace(pLen);

        typename E::child_type::first_type cname(pLen, '\0');

        is.read(cname.data(), std::streamsize(pLen));

        typename E::child_type::second_type ckey;

        is.read(reinterpret_cast<char*>(&ckey), sizeof (ckey));
        be::little_to_native_inplace(ckey);

        E child(ckey, cname);

        auto status = ret.addChild(child);

        assert(status.isOk());
    }

    p = std::move(ret);

    return is;
}

template <typename Key,
          Key TInvalidKey,
          typename PropertyName,
          typename PropertyValue,
          typename PropertyContainer>
inline std::ostream& operator<<(std::ostream& os, const Entry<Key, TInvalidKey, PropertyName, PropertyValue, PropertyContainer> & p) {
    namespace be = boost::endian;

    auto key = p.key();
    auto parent = p.parent();
    auto name = p.name();
    std::uint64_t nameLength = name.size();
    auto properties = p.propertiesSet();
    std::uint64_t propertiesCount = properties.size();
    auto children = p.children();
    std::uint64_t childrenCount = children.size();

    be::native_to_little_inplace(key);
    be::native_to_little_inplace(parent);
    be::native_to_little_inplace(nameLength);
    be::native_to_little_inplace(propertiesCount);
    be::native_to_little_inplace(childrenCount);

    os.write(reinterpret_cast<const char*>(&key), sizeof(key));
    os.write(reinterpret_cast<const char*>(&parent), sizeof(parent));
    os.write(reinterpret_cast<const char*>(&nameLength), sizeof (nameLength));
    os.write(name.data(), name.size());

    os.write(reinterpret_cast<const char*>(&propertiesCount), sizeof (propertiesCount));

    for (const auto& prop : properties) {
        const auto& [status, value] = p.property(prop);

        assert(status.isOk());

        decltype(nameLength) pLen = prop.size();

        be::native_to_little_inplace(pLen);

        os.write(reinterpret_cast<const char*>(&pLen), sizeof(pLen));
        os.write(prop.data(), std::streamsize(prop.size()));
        os << value;
    }

    os.write(reinterpret_cast<const char*>(&childrenCount), sizeof (childrenCount));

    for (const auto& c : children) {
        const auto& [name, key] = c;
        decltype(nameLength) pLen = name.size();

        be::native_to_little_inplace(pLen);
        os.write(reinterpret_cast<const char*>(&pLen), sizeof(pLen));
        os.write(name.data(), name.size());

        be::native_to_little_inplace(key);
        os.write(reinterpret_cast<const char*>(&key), sizeof(key));
    }

    return os;
}

}
