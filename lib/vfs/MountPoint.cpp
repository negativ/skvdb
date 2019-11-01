#include "MountPoint.hpp"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/tag.hpp>

#include "util/StringPath.hpp"

namespace skv::vfs {

struct MountPoint::Impl {
    std::string mountPath_;
};

MountPoint::MountPoint(std::string_view mountPath):
    impl_{new Impl{util::simplifyPath(mountPath)}}
{

}

MountPoint::~MountPoint() noexcept {

}

}
