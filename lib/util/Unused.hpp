#pragma once

template <typename T>
static inline void SKV_UNUSED(T&& t) {
    static_cast<void>(t);
}
