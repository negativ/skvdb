#include "File.hpp"

#ifdef BUILDING_UNIX

namespace skv::os {

File::Handle File::open(std::string_view path, std::string_view mode) noexcept {
    return File::Handle{::fopen(path.data(), mode.data()), &(::fclose)};
}

std::int64_t File::tell(Handle fhandle) noexcept {
    if (!fhandle)
        return -1;

    return ::ftello(fhandle.get());
}

std::uint64_t File::write(const void* __restrict ptr, std::uint64_t size, std::uint64_t n, Handle handle) noexcept {
    if (!handle)
        return 0;

    return ::fwrite(ptr, size, n, handle.get());
}

std::uint64_t File::read(void* __restrict ptr, std::uint64_t size, std::uint64_t n, Handle handle) noexcept {
    if (!handle)
        return 0;

    return ::fread(ptr, size, n, handle.get());
}

bool File::seek(Handle handle, std::int64_t offset, Seek s) noexcept {
    if (!handle)
        return -1;

    return (::fseeko(handle.get(), offset, static_cast<int>(s)) == 0);
}

void File::flush(Handle handle) noexcept {
    ::fflush(handle.get());
}

}

#endif
