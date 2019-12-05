#include "File.hpp"

#ifdef BUILDING_WINDOWS

#include <windows.h>

namespace skv::os {

    File::Handle File::open(const fs::path& path, std::string_view mode) noexcept {
		const auto& pathStr = path.string();

        return File::Handle{::fopen(pathStr.c_str(), mode.data()),
                            [](FILE* f) -> int {
                                if (f)
                                    return ::fclose(f);
                                return -1;
                            }};
    }

    std::int64_t File::tell(const Handle& fhandle) noexcept {
        if (!fhandle)
            return -1;

        return ::_ftelli64(fhandle.get());
    }

    std::uint64_t File::write(const void* __restrict ptr, std::uint64_t size, std::uint64_t n, const Handle& handle) noexcept {
        if (!handle)
            return 0;

        return ::fwrite(ptr, std::size_t(size), std::size_t(n), handle.get());
    }

    std::uint64_t File::read(void* __restrict ptr, std::uint64_t size, std::uint64_t n, const Handle& handle) noexcept {
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

    bool File::unlink(const fs::path& path) noexcept {
		const auto& pathStr = path.string();

        return ::DeleteFile(pathStr.c_str()) != 0;
    }

    bool File::exists(const fs::path& filePath) noexcept {
        return fs::exists(filePath);
    }

    bool File::rename(const fs::path& oldName, const fs::path& newName) noexcept {
		const auto& oldPathStr = oldName.string();
		const auto& newPathStr = newName.string();

        return ::rename(oldPathStr.c_str(), newPathStr.c_str()) == 0;
    }

    char File::sep() noexcept {
        return fs::path::separator;
    }

}

#endif
