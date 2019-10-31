#pragma once

#include <sstream>
#include <string_view>
#include <type_traits>

namespace skv::util {

struct Log final {
    Log() = delete;
    ~Log() = delete;
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;

#ifndef NDEBUG
    template <typename ... Ts>
    static inline void d(std::string_view tag, Ts ... args) {
        std::stringstream stream;

        ((stream << to_string(args)), ...);

        Log::d_(tag, stream.str());
    }
#else
    template <typename ... Ts>
    static inline void d(std::string_view, Ts ...) {
    }
#endif

    template <typename ... Ts>
    static inline void w(std::string_view tag, Ts ... args) {
        std::stringstream stream;

        ((stream << to_string(args)), ...);

        Log::w_(tag, stream.str());
    }

    template <typename ... Ts>
    static inline void i(std::string_view tag, Ts ... args) {
        std::stringstream stream;

        ((stream << to_string(args)), ...);

        Log::i_(tag, stream.str());
    }

    template <typename ... Ts>
    static inline void e(std::string_view tag, Ts ... args) {
        std::stringstream stream;

        ((stream << to_string(args)), ...);

        Log::e_(tag, stream.str());
    }

private:
    template <typename T>
    static inline std::string to_string(T&& arg) {
        if constexpr (std::is_same_v<std::string, std::decay_t<T>>) {
            return arg;
        }

        return std::to_string(arg) + " ";
    }

    static inline std::string to_string(const char *str) {
        return str;
    }

    static void d_(std::string_view tag, std::string_view msg);
    static void w_(std::string_view tag, std::string_view msg);
    static void i_(std::string_view tag, std::string_view msg);
    static void e_(std::string_view tag, std::string_view msg);

};

}
