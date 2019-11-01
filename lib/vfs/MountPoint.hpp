#pragma once

#include <memory>
#include <string_view>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/indexed_by.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/mem_fun.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/tag.hpp>

#include "MountPointEntry.hpp"

namespace skv::vfs::mount {

namespace tags {
    struct ByEntry{};
    struct ByMountPoint{};
    struct ByPathAndVolume{};
}

using namespace boost::multi_index;

using Points = multi_index_container<Entry,
                                     indexed_by<random_access<tag<tags::ByEntry>>,
                                                ordered_non_unique<tag<tags::ByMountPoint>,
                                                                   const_mem_fun<Entry, std::string, &Entry::mountPath>>,
                                                hashed_unique<tag<tags::ByPathAndVolume>,
                                                              composite_key<Entry,
                                                                            const_mem_fun<Entry, std::string, &Entry::entryPath>,
                                                                            const_mem_fun<Entry, IVolumePtr, &Entry::volume>>>>>;

}
