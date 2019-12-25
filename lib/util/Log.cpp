#include "Log.hpp"
#include "Unused.hpp"
#include <iostream>

namespace skv::util {

void Log::d_(std::string_view tag, std::string_view msg) {
#ifndef NDEBUG
    std::cout << "[" << tag.data() << "/D]: " << msg << std::endl;
#else
    SKV_UNUSED(tag);
    SKV_UNUSED(msg);
#endif
}

void Log::w_(std::string_view tag, std::string_view msg) {
    std::cout << "[" << tag.data() << "/W]: " << msg << std::endl;
}

void Log::i_(std::string_view tag, std::string_view msg) {
    std::cout << "[" << tag.data() << "/I]: " << msg << std::endl;
}

void Log::e_(std::string_view tag, std::string_view msg) {
    std::cerr << "[" << tag.data() << "/E]: " << msg << std::endl;
}

}
