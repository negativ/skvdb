#pragma once

#include <memory>
#include <string_view>

#include "MountPointEntry.hpp"

namespace skv::vfs {

class MountPoint final {
    struct Impl;

public:
    MountPoint(std::string_view mountPath);
    ~MountPoint() noexcept;

private:
    std::unique_ptr<Impl> impl_;
};

}
