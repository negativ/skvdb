#pragma once

#include <memory>

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
    struct ByAllRA{};       // For random-access
    struct ByMountPath{};   // For mount path search
    struct ByAll{};         // For search by mount path & volume & entry path
}

using namespace boost::multi_index;

using Points = multi_index_container<Entry,
                                     indexed_by<random_access<tag<tags::ByAllRA>>,
                                                ordered_non_unique<tag<tags::ByMountPath>,
                                                                   const_mem_fun<Entry, std::string, &Entry::mountPath>>,
                                                hashed_unique<tag<tags::ByAll>,
                                                              composite_key<Entry,
                                                                            const_mem_fun<Entry, std::string, &Entry::entryPath>,
                                                                            const_mem_fun<Entry, IVolumePtr, &Entry::volume>,
                                                                            const_mem_fun<Entry, std::string, &Entry::mountPath>>>>>;

}
