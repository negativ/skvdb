#include "File.hpp"

#ifdef BUILDING_WINDOWS

#include <windows.h>

namespace skv::os {

    File::Handle File::open(std::string_view path, std::string_view mode) noexcept {
        FILE* handle;

        if (::fopen_s(&handle, path.data(), mode.data()) == 0)
            return File::Handle{handle,
                            [](FILE* f) -> int {
                                if (f)
                                    return ::fclose(f);
                                return -1;
                               }};

        return File::Handle{};
    }

    std::int64_t File::tell(const Handle& fhandle) noexcept {
        if (!fhandle)
            return -1;

        return ::_ftelli64(fhandle.get());
    }

    std::uint64_t File::write(const void* __restrict ptr, std::uint64_t size, std::uint64_t n, Handle handle) noexcept {
        if (!handle)
            return 0;

        return ::fwrite(ptr, std::size_t(size), std::size_t(n), handle.get());
    }

    std::uint64_t File::read(void* __restrict ptr, std::uint64_t size, std::uint64_t n, Handle handle) noexcept {
        if (!handle)
            return 0;

        return ::fread(ptr,  std::size_t(size),  std::size_t(n), handle.get());
    }

    bool File::seek(const Handle &handle, std::int64_t offset, Seek s) noexcept {
        if (!handle)
            return false;

        return (::_fseeki64(handle.get(), offset, static_cast<int>(s)) == 0);
    }

    void File::flush(const Handle &handle) noexcept {
        ::fflush(handle.get());
    }

    bool File::unlink(std::string_view filePath) noexcept {
        return ::DeleteFile(filePath.data()) == 0;
    }

    bool File::exists(std::string_view filePath) noexcept {
        DWORD dwAttrib = GetFileAttributes(filePath.data());

        return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
                !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
    }

    char File::sep() noexcept {
        return '\\';
    }

}

#endif
