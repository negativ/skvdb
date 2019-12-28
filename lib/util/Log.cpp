#include "Log.hpp"
#include <iostream>

namespace skv::util {

void Log::d_([[maybe_unused]] std::string_view tag, [[maybe_unused]] std::string_view msg) {
#ifndef NDEBUG
    std::cout << "[" << tag.data() << "/D]: " << msg << std::endl;
#else
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
