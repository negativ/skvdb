#include "Property.hpp"

#include <type_traits>
#include <boost/endian/conversion.hpp>

namespace skv::ondisk {

template<typename VariantType, typename T, const std::size_t index = 0>
constexpr std::size_t getIndex() {
    if constexpr (index == std::variant_size_v<VariantType>) {
        return index;
    } else if constexpr (std::is_same_v<std::variant_alternative_t<index, VariantType>, T>) {
        return index;
    } else {
        return getIndex<VariantType, T, index + 1>();
    }
}

std::ostream& operator<<(std::ostream& os, const Property& p) {
    namespace be = boost::endian;

    std::uint16_t idx = p.index() & 0xFFFF;
    be::native_to_little_inplace(idx);

    os.write(reinterpret_cast<const char*>(&idx), sizeof(idx));

    std::visit([&os](auto&& v) {
        using type = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<type, std::string>) {
            std::uint64_t size = v.size();
            be::native_to_little_inplace(size);

            os.write(reinterpret_cast<const char*>(&size), sizeof(size));
            os.write(v.data(), std::streamsize(v.size()));
        }
        else {
            be::native_to_little_inplace(v);
            os.write(reinterpret_cast<const char*>(&v), sizeof(v));
        }
    }, p);
    return os;
}

template <typename T>
void readProperty(std::istream& is, uint16_t idx, Property& p) {
    namespace be = boost::endian;

    if (getIndex<Property, T>() == idx) {
        if constexpr (std::is_same_v<T, std::string>) {
            std::uint64_t size;

            is.read(reinterpret_cast<char*>(&size), sizeof(size));

            be::little_to_native_inplace(size);
            std::string ret(std::size_t(size), '\0');

            is.read(ret.data(), std::streamsize(size));

            p = Property{ret};
        }
        else {
            T v;

            is.read(reinterpret_cast<char*>(&v), sizeof(v));
            be::little_to_native_inplace(v);

            p = Property{v};
        }
    }
}

template <typename ... Ts>
void readProperty(std::istream& is, uint16_t idx, std::variant<Ts...> &p) {
    (readProperty<Ts>(is, idx, p), ...);
}

std::istream& operator>>(std::istream& is, Property& p) {
    namespace be = boost::endian;

    std::uint16_t idx;

    is.read(reinterpret_cast<char*>(&idx), sizeof(idx));

    be::native_to_little_inplace(idx);

    readProperty(is, idx, p);

    return is;
}

}

