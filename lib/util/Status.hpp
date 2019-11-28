#pragma once

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


public:
    [[nodiscard]] static Status Ok();
    [[nodiscard]] static Status IOError(const char* message);
    [[nodiscard]] static Status InvalidArgument(const char* message);
    [[nodiscard]] static Status NotFound(const char* message);
    [[nodiscard]] static Status Fatal(const char* message);
    [[nodiscard]] static Status InvalidOperation(const char* message);

    Status() = default;
    ~Status() = default;

    Status(const Status&) = default;
    Status& operator=(const Status&) = default;

    Status(Status&&) noexcept = default;
    Status& operator=(Status&&) noexcept = default;

    [[nodiscard]] const char* message() const noexcept;

    [[nodiscard]] Code code() const noexcept;

    [[nodiscard]] bool isOk() const noexcept;
    [[nodiscard]] bool isIOError() const noexcept;
    [[nodiscard]] bool isInvalidArgument() const noexcept;
    [[nodiscard]] bool isNotFound() const noexcept;
    [[nodiscard]] bool isFatal() const noexcept;
    [[nodiscard]] bool isInvalidOperation() const noexcept;

private:
    Status(Code code, const char* msg = nullptr) noexcept;

    Code code_{Code::Undefined};
    const char* message_{nullptr};
};

}
