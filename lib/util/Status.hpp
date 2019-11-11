#pragma once

#include <string>
#include <type_traits>

namespace skv::util {

/**
 * @brief Operation status indication
 */
class Status final {
    enum class Code {
        Ok,
        IOError,
        InvalidArgument,
        NotFound,
        Fatal,
        InvalidOp,
        Undefined
    };

    Code code_{Code::Undefined};
    std::string message_;

    static constexpr bool noexcept_move_constructible = std::is_nothrow_move_constructible_v<decltype(message_)>;
    static constexpr bool noexcept_move_assignable = std::is_nothrow_move_assignable_v<decltype(message_)>;

public:
    [[nodiscard]] static Status Ok();
    [[nodiscard]] static Status IOError(std::string message);
    [[nodiscard]] static Status InvalidArgument(std::string message);
    [[nodiscard]] static Status NotFound(std::string message);
    [[nodiscard]] static Status Fatal(std::string message);
    [[nodiscard]] static Status InvalidOperation(std::string message);

    Status() = default;
    ~Status() = default;

    Status(const Status&) = default;
    Status& operator=(const Status&) = default;

    Status(Status&&) noexcept(noexcept_move_constructible) = default;
    Status& operator=(Status&&) noexcept(noexcept_move_assignable) = default;

    [[nodiscard]] std::string message() const noexcept(std::is_nothrow_copy_constructible_v<decltype(message_)>);

    [[nodiscard]] Code code() const noexcept;

    [[nodiscard]] bool isOk() const noexcept;
    [[nodiscard]] bool isIOError() const noexcept;
    [[nodiscard]] bool isInvalidArgument() const noexcept;
    [[nodiscard]] bool isNotFound() const noexcept;
    [[nodiscard]] bool isFatal() const noexcept;
    [[nodiscard]] bool isInvalidOperation() const noexcept;

private:
    Status(Code code, std::string message) noexcept(noexcept_move_constructible);
};

}
