cmake_minimum_required(VERSION 2.8)

project(skvdb CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

find_package(Boost 1.60 REQUIRED COMPONENTS filesystem system)

include_directories(${Boost_INCLUDE_DIRS})

if (UNIX)
    add_definitions(-DBUILDING_UNIX)
elseif(WIN32)
    add_definitions(-DBUILDING_WINDOWS)
endif ()

add_subdirectory(lib)
add_subdirectory(tests)
