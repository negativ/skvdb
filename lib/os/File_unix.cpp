#include "File.hpp"

#ifdef BUILDING_UNIX

#include <unistd.h>

namespace skv::os {

File::Handle File::open(std::string_view path, std::string_view mode) noexcept {
    return File::Handle{::fopen(path.data(), mode.data()),
                        [](FILE* f) -> int {
                            if (f)
                                return ::fclose(f);
                            return -1;
                        }};
}

std::int64_t File::tell(const Handle& fhandle) noexcept {
    if (!fhandle)
        return -1;

    return ::ftello(fhandle.get());
}

std::uint64_t File::write(const void* __restrict ptr, std::uint64_t size, std::uint64_t n, const Handle& handle) noexcept {
    if (!handle)
        return 0;

    return ::fwrite(ptr, size, n, handle.get());
}

std::uint64_t File::read(void* __restrict ptr, std::uint64_t size, std::uint64_t n, const Handle& handle) noexcept {
    if (!handle)
        return 0;

    return ::fread(ptr, size, n, handle.get());
}

bool File::seek(const Handle &handle, std::int64_t offset, Seek s) noexcept {
    if (!handle)
        return false;

    return (::fseeko(handle.get(), offset, static_cast<int>(s)) == 0);
}

void File::flush(const Handle &handle) noexcept {
    ::fflush(handle.get());
}

bool File::unlink(std::string_view filePath) noexcept {
    return ::unlink(filePath.data()) == 0;
}

bool File::exists(std::string_view filePath) noexcept {
    return ::access(filePath.data(), F_OK) != -1;
}

char File::sep() noexcept {
    return '/';
}

bool File::rename(std::string_view oldName, std::string newName) noexcept {
    return ::rename(oldName.data(), newName.data()) == 0;
}

}

#endif
