#pragma once

#include <shared_mutex>

#include "Record.hpp"
#include "vfs/IEntry.hpp"
#include "util/Status.hpp"

namespace skv::ondisk {

class Entry final: public skv::vfs::IEntry {
public:
    Entry(ondisk::Record&& record) noexcept;

    ~Entry() noexcept override = default;

    Entry(const Entry&) = delete;
    Entry& operator=(const Entry&) = delete;

    Entry(Entry&&) = delete;
    Entry& operator=(Entry&&) = delete;

    Handle handle() const noexcept override;


    std::tuple<Status, bool> hasProperty(const std::string &prop) const noexcept override;

    Status setProperty(const std::string &prop, const Property &value) override;

    std::tuple<Status, Property> property(const std::string &prop) const override;

    Status removeProperty(const std::string &prop) override;

    std::tuple<Status, Properties> properties() const override;

    std::tuple<Status, std::set<std::string>> propertiesNames() const override;

    Status expireProperty(const std::string &prop, chrono::milliseconds ms) override;

    Status cancelPropertyExpiration(const std::string &prop) override;

    std::tuple<Status, std::set<std::string>> children() const override;

    void setDirty(bool dirty) noexcept;

    [[nodiscard]] bool dirty() const noexcept;

    [[nodiscard]] Record& record() const noexcept;

    [[nodiscard]] std::shared_mutex& xLock() const noexcept;

private:
    mutable Record record_;
    mutable std::shared_mutex xLock_;
    bool dirty_{false};
};

}
