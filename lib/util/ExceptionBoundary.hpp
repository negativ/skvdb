#pragma once

#include <cassert>
#include <stdexcept>

#include "Log.hpp"
#include "Status.hpp"

namespace skv::util {

template <typename F>
inline Status exceptionBoundary(const char* TAG, F&& call) {
    assert(TAG != nullptr);

    try {
        call();

        return Status::Ok();
    }
    catch (const std::bad_alloc&) { return Status::Fatal("bad_alloc"); }
    catch (const std::exception& e) {
        Log::e(TAG, e.what());
    }
    catch (...) {
        Log::e(TAG, "Unknown exception");
    }

    return Status::Fatal("Exception");
}

}
