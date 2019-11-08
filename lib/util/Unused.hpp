#pragma once

template <typename T>
inline void SKV_UNUSED(T&& t) {
    static_cast<void>(t);
}
