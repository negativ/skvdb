#include "Property.hpp"

#include <type_traits>
#include "util/Serialization.hpp"

namespace std {

using namespace skv;
using namespace skv::vfs;

template<typename VariantType, typename T, const std::size_t index = 0>
constexpr std::size_t variantTypeIndex() {
    if constexpr (index == std::variant_size_v<VariantType>) {
        return index;
    }
    else if constexpr (std::is_same_v<std::variant_alternative_t<index, VariantType>, T>) {
        return index;
    }
    else {
        return variantTypeIndex<VariantType, T, index + 1>();
    }
}

template <typename T>
void readProperty(std::istream& _is, uint16_t idx, Property& p) {
    util::Deserializer ds{_is};

    if (variantTypeIndex<Property, T>() == idx) {
        T ret;

        ds >> ret;

        p = Property{ret};
    }
}

template <typename ... Ts>
void readProperty(std::istream& is, uint16_t idx, std::variant<Ts...> &p) {
    (readProperty<Ts>(is, idx, p), ...);
}

std::ostream& operator<<(std::ostream& _os, const Property& p) {
    util::Serializer s{_os};

    std::uint16_t idx = p.index() & PropertyIndexMask;
    s << idx;

    std::visit([&s](auto&& v) { s << v; }, p);

    return _os;
}

std::istream& operator>>(std::istream& _is, Property& p) {
    util::Deserializer ds{_is};

    std::uint16_t idx;
    ds >> idx;

    readProperty(_is, idx, p);

    return _is;
}

}

