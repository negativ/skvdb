#pragma once

#include <cstdint>
#include <memory>
#include <cstdio>

#include <boost/filesystem.hpp>

#include <util/Status.hpp>

namespace skv::os {

namespace fs = boost::filesystem;

/**
 * @brief OS-specific low-level actions with files
 */
struct File {
    enum class Seek: int {
        Set = SEEK_SET,
        End = SEEK_END,
        Cur = SEEK_CUR
    };

    using Handle = std::shared_ptr<std::FILE>;

    [[nodiscard]] static Handle open(const fs::path& path, std::string_view mode) noexcept;

    [[nodiscard]] static std::uint64_t write(const void* __restrict ptr, std::uint64_t size, std::uint64_t n, const Handle& handle) noexcept;
    [[nodiscard]] static std::uint64_t read(void* __restrict ptr, std::uint64_t size, std::uint64_t n, const Handle& handle) noexcept;

    [[nodiscard]] static bool seek(const Handle& handle, std::int64_t offset, Seek s) noexcept;
    [[nodiscard]] static std::int64_t tell(const Handle& fhandle) noexcept;

    static void flush(const Handle& handle) noexcept;

    [[nodiscard]] static bool unlink(const fs::path& filePath) noexcept;

    [[nodiscard]] static bool exists(const fs::path& filePath) noexcept;

    [[nodiscard]] static char sep() noexcept;  // path separator

    [[nodiscard]] static bool rename(const fs::path& oldName, const fs::path& newName) noexcept;
};



}
