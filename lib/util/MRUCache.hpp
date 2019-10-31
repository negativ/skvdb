#pragma once

#include <mutex>
#include <type_traits>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/tag.hpp>

#include "util/SpinLock.hpp"

namespace skv::util {

namespace bmi = boost::multi_index;

template <typename Key, typename Value, std::size_t Capacity = 512>
class MRUCache final {
    struct ByKey{};
public:
    static constexpr std::size_t capacity_value = Capacity;

    static_assert (capacity_value != 0, "MRU capacity should be > 0");

    using key_type          = std::decay_t<Key>;
    using value_type        = std::decay_t<Value>;
    using cache_item_type   = std::pair<key_type, value_type>;
    using cache_type        = bmi::multi_index_container<cache_item_type,
                                                         bmi::indexed_by<
                                                             bmi::sequenced<>,
                                                             bmi::hashed_unique<bmi::tag<ByKey>,
                                                                                bmi::member<cache_item_type,
                                                                                            typename cache_item_type::first_type,
                                                                                            &cache_item_type::first>>>>;
    using iterator = typename cache_type::iterator;
    using const_iterator = typename cache_type::const_iterator;

    void insert(const key_type& key, const value_type& value) {
        std::lock_guard locker(xLock_);

        auto result = cache_.push_front(cache_item_type{key, value});

        if (!result.second)
            cache_.relocate(std::begin(cache_), result.first);
        else if (cache_.size() > capacity())
            cache_.pop_back();
    }

    [[nodiscard]] bool lookup(const key_type& key, value_type& value) {
        std::lock_guard locker(xLock_);

        auto& index = cache_.template get<ByKey>();

        auto it = index.find(key);

        if (it == std::end(index)) {
            ++cacheMissCount_;

            return false;
        }

        ++cacheHitCount_;

        value = it->second;

        auto seqit = cache_.template project<0>(it);
        cache_.relocate(std::begin(cache_), seqit);

        return true;
    }

    [[nodiscard]] bool remove(const key_type& key) {
        std::lock_guard locker(xLock_);

        auto& index = cache_.template get<ByKey>();

        auto it = index.find(key);

        if (it == std::end(index))
            return false;

        index.erase(it);

        return true;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        std::lock_guard locker(xLock_);

        return cache_.size();
    }

    [[nodiscard]] constexpr std::size_t capacity() const noexcept {
        return capacity_value;
    }

    [[nodiscard]] std::uint64_t cacheHitCount() const noexcept {
        return cacheHitCount_;
    }

    [[nodiscard]] std::uint64_t cacheMissCount() const noexcept {
        return cacheHitCount_;
    }

private:
    mutable SpinLock xLock_; // spinlock should be good because all operations on MRU are very fast
    cache_type cache_;
    std::uint64_t cacheMissCount_{0};
    std::uint64_t cacheHitCount_{0};
};

}

