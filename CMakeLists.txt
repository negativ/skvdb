cmake_minimum_required(VERSION 2.8)

project(skvdb CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

if (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-long-long -pedantic -Wno-psabi  -fdata-sections -ffunction-sections")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -Wall -Wno-long-long -pedantic -Wno-psabi -s -fdata-sections -ffunction-sections")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wno-long-long -pedantic -Wno-psabi -fdata-sections -ffunction-sections")

    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,--gc-sections")
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "${CMAKE_SHARED_LINKER_FLAGS_DEBUG} -Wl,--gc-sections")
endif ()

find_package(Boost 1.60 REQUIRED)

include_directories(${Boost_INCLUDE_DIRS})
add_definitions(${Boost_LIB_DIAGNOSTIC_DEFINITIONS})

if (UNIX)
    add_definitions(-DBUILDING_UNIX)
elseif(WIN32)
    add_definitions(-DBUILDING_WINDOWS)
endif ()

add_subdirectory(lib)
add_subdirectory(tests)
