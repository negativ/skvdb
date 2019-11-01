#include "Storage.hpp"

#include "MountPoint.hpp"

namespace skv::vfs {

struct Storage::Impl {
    mount::Points mpoints_{};
};

Storage::Storage():
    impl_{new Impl{}}
{

}

Storage::~Storage() noexcept {

}

std::tuple<Status, Storage::Handle> Storage::open(std::string_view path) {
}

Status Storage::close(Storage::Handle d) {

}

std::tuple<Status, Storage::Links> Storage::links(Storage::Handle h) {

}

std::tuple<Status, Storage::Properties > Storage::properties(Storage::Handle handle) {

}

std::tuple<Status, Property> Storage::property(Storage::Handle h, std::string_view name) {

}

Status Storage::setProperty(Storage::Handle h, std::string_view name, const Property &value) {

}

Status Storage::removeProperty(Storage::Handle h, std::string_view name) {

}

std::tuple<Status, bool> Storage::hasProperty(Storage::Handle h, std::string_view name) {

}

Status Storage::expireProperty(Storage::Handle h, std::string_view name, chrono::system_clock::time_point tp) {

}

Status Storage::cancelPropertyExpiration(Storage::Handle h, std::string_view name) {

}

Status Storage::link(Storage::Handle h, std::string_view name) {

}

Status Storage::unlink(Handle h, std::string_view name) {

}

}

