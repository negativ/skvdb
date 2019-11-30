#pragma once

#include <array>

namespace skv::util {

/**
 * @brief Operation status indication
 */
class Status final {
    enum class Code: unsigned {
        Ok,
        IOError,
        InvalidArgument,
        NotFound,
        Fatal,
        InvalidOp,
        Undefined
    };

    static constexpr std::size_t MAX_MESSAGE_LEN = 36;

public:
    [[nodiscard]] static Status Ok();

    template<typename T>
    [[nodiscard]] static constexpr Status IOError(T&& m) {
        return create(Code::IOError, std::forward<T>(m));
    }

    template<typename T>
    [[nodiscard]] static constexpr Status InvalidArgument(T&& m) {
        return create(Code::InvalidArgument, std::forward<T>(m));
    }

    template<typename T>
    [[nodiscard]] static constexpr Status NotFound(T&& m) {
        return create(Code::NotFound, std::forward<T>(m));
    }

    template<typename T>
    [[nodiscard]] static constexpr Status Fatal(T&& m) {
        return create(Code::Fatal, std::forward<T>(m));
    }

    template<typename T>
    [[nodiscard]] static constexpr Status InvalidOperation(T&& m) {
        return create(Code::InvalidOp, std::forward<T>(m));
    }

    constexpr Status() = default;
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
    template<std::size_t N>
    [[nodiscard]] static inline constexpr Status create(Code code, const char (&m)[N]) {
        static_assert(N < MAX_MESSAGE_LEN, "Message too long. Max length is 36 chars");

        return Status{code, m};
    }

    template<typename Array, typename String, std::size_t... I>
    constexpr void str2array(Array& a, const String& s, std::index_sequence<I...>)
    {
        ((a[I] = s[I]), ...);
    }

    template<std::size_t N, typename Indices = std::make_index_sequence<N>>
    constexpr Status(Code code, const char (&m)[N]) noexcept:
        code_(code)
    {
        str2array(message_, m, Indices{});
    }

    Code code_{Code::Undefined};
    std::array<char, MAX_MESSAGE_LEN> message_{0};
};

}
