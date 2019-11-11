#pragma once

#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <type_traits>

namespace skv::ondisk {

/**
 * @brief Entry control block. Used by ondisk::Volume
 */
template <typename EntryType>
class ControlBlock final {
public:
    using entry_type = std::decay_t<EntryType>;
    using ptr        = std::shared_ptr<ControlBlock<entry_type>>;

    template <typename ... Ts>
    static ptr create(Ts&& ... args) {
        return ptr{new ControlBlock{std::forward<Ts>(args)...}};
    }

    ~ControlBlock() = default;

    ControlBlock(const ControlBlock&) = delete;
    ControlBlock& operator=(const ControlBlock&) = delete;
    ControlBlock(ControlBlock&&) = delete;
    ControlBlock& operator=(ControlBlock&&) = delete;

    [[nodiscard]] entry_type& entry() const noexcept {
        return entry_;
    }

    [[nodiscard]] std::shared_mutex& xLock() const noexcept {
        return xLock_;
    }

    void claim() noexcept {
        ++usageCounter_;
    }

    void release() noexcept {
        --usageCounter_;
    }

    [[nodiscard]] bool free() const noexcept {
        return usageCounter_ == 0;
    }

    void setDirty(bool d) noexcept {
        dirty_ = d;
    }

    [[nodiscard]] bool dirty() const noexcept {
        return dirty_;
    }

private:
    ControlBlock() = default;
    ControlBlock(entry_type&& e):
        usageCounter_{0},
        entry_{e},
        dirty_{false}
    {}

    mutable std::shared_mutex xLock_;
    std::uint64_t usageCounter_{0};
    mutable entry_type entry_;
    bool dirty_{false};
};

}
