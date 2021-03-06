#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <boost/endian/conversion.hpp>

namespace skv::util {

struct Serializer final {
    Serializer(std::ostream& os) noexcept:
        os{os}
    {}

    ~Serializer() noexcept  = default;

    Serializer(const Serializer&) = delete;
    Serializer& operator=(const Serializer&) = delete;

    Serializer(Serializer&&) = delete;
    Serializer& operator=(Serializer&&) = delete;

    template <typename T>
    inline Serializer& operator<<(const T& p)
    {
        namespace be = boost::endian;

        if constexpr (std::is_floating_point_v<T>) {
            union {
                T             fvalue;
                std::uint64_t ivalue{0};
            } value;

            value.fvalue = p;

            auto x = be::native_to_little(value.ivalue);
            os.write(reinterpret_cast<const char*>(&x),  sizeof(x));
        }
        else if constexpr (std::is_integral_v<T>) {
            auto x = be::native_to_little(p);
            os.write(reinterpret_cast<const char*>(&x),  sizeof(x));
        }
        else if constexpr (std::is_same_v<T, std::string> ||
                           std::is_same_v<T, std::string_view> ||
                           std::is_same_v<T, std::vector<char>>) {
            auto x = be::native_to_little(std::uint64_t(p.size()));

            os.write(reinterpret_cast<const char*>(&x), sizeof(x));
            os.write(p.data(), std::streamsize(p.size()));
        }
        else
            os << p;

        return *this;
    }

private:
    std::ostream& os;
};

struct Deserializer final {
    Deserializer(std::istream& is) noexcept:
        is{is}
    {}

    ~Deserializer() noexcept  = default;

    Deserializer(const Deserializer&) = delete;
    Deserializer& operator=(const Deserializer&) = delete;

    Deserializer(Deserializer&&) = delete;
    Deserializer& operator=(Deserializer&&) = delete;

    template <typename T>
    inline Deserializer& operator>>(T& p)
    {
        namespace be = boost::endian;

        if constexpr (std::is_floating_point_v<T>) {
            union {
                T             fvalue;
                std::uint64_t ivalue;
            } value;

            is.read(reinterpret_cast<char*>(&value), sizeof(value));
            be::little_to_native_inplace(value.ivalue);

            p = value.fvalue;
        }
        else if constexpr (std::is_integral_v<T>) {
            is.read(reinterpret_cast<char*>(&p), sizeof(p));
            be::little_to_native_inplace(p);
        }
        else if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::vector<char>>) {
            std::uint64_t pLen;
            is.read(reinterpret_cast<char*>(&pLen), sizeof(pLen));

            be::little_to_native_inplace(pLen);

            T ret(std::size_t(pLen), '\0');

            is.read(ret.data(), std::streamsize(pLen));

            p = std::move(ret);
        }
        else
            is >> p;

        return *this;
    }

private:
    std::istream& is;
};

}
