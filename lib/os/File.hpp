#pragma once

#include <cstdint>
#include <memory>
#include <string_view>
#include <cstdio>

#include <util/Status.hpp>

namespace skv::os {

/**
 * @brief OS-specific file low-level actions
 */
struct File {
    enum class Seek: int {
        Set = SEEK_SET,
        End = SEEK_END,
        Cur = SEEK_CUR
    };

    using Handle = std::shared_ptr<std::FILE>;

    [[nodiscard]] static Handle open(std::string_view path, std::string_view mode) noexcept;

    [[nodiscard]] static std::uint64_t write(const void* __restrict ptr, std::uint64_t size, std::uint64_t n, const Handle& handle) noexcept;
    [[nodiscard]] static std::uint64_t read(void* __restrict ptr, std::uint64_t size, std::uint64_t n, const Handle& handle) noexcept;

    [[nodiscard]] static bool seek(const Handle& handle, std::int64_t offset, Seek s) noexcept;
    [[nodiscard]] static std::int64_t tell(const Handle& fhandle) noexcept;

    static void flush(const Handle& handle) noexcept;

    [[nodiscard]] static bool unlink(std::string_view filePath) noexcept;

    [[nodiscard]] static bool exists(std::string_view filePath) noexcept;

    [[nodiscard]] static char sep() noexcept;  // path separator

    [[nodiscard]] static bool rename(std::string_view oldName, std::string newName) noexcept;
};



}
