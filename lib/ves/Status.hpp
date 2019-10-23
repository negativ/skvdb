#pragma once

#include <string>
#include <type_traits>

namespace skv::ves {

class Status final {
    enum class Code {
        Ok,
        IOError,
        InvalidArgument,
        NotFound
    };

    Code code_;
    std::string message_;

    static constexpr bool noexcept_copy_constructible = std::is_nothrow_copy_constructible_v<decltype(message_)>;
    static constexpr bool noexcept_copy_assignable = std::is_nothrow_copy_assignable_v<decltype(message_)>;
    static constexpr bool noexcept_move_constructible = std::is_nothrow_move_constructible_v<decltype(message_)>;
    static constexpr bool noexcept_move_assignable = std::is_nothrow_move_assignable_v<decltype(message_)>;

public:
    static Status OK() noexcept(noexcept_copy_constructible);
    static Status IOError(std::string message) noexcept(noexcept_copy_constructible);
    static Status InvalidArgument(std::string message) noexcept(noexcept_copy_constructible);
    static Status NotFound(std::string message) noexcept(noexcept_copy_constructible);

    ~Status() = default;

    Status(const Status&) noexcept(noexcept_copy_constructible) = default;
    Status& operator=(const Status&) noexcept(noexcept_copy_assignable) = default;

    Status(Status&&) noexcept(noexcept_move_constructible) = default;
    Status& operator=(Status&&) noexcept(noexcept_move_assignable) = default;

    std::string message() const noexcept(std::is_nothrow_copy_constructible_v<decltype(message_)>);

    Code code() const noexcept;

    bool isOk() const noexcept;
    bool isIOError() const noexcept;
    bool isInvalidArgument() const noexcept;
    bool isNotFound() const noexcept;
private:
    Status(Code code, std::string message) noexcept(noexcept_move_constructible);
};

}
