cmake_minimum_required(VERSION 2.8)

project(skv CXX)

set(CMAKE_CXX_STANDARD 17)
set(VERSION "0.0.1")

find_package (Git)

if (GIT_FOUND)
    execute_process(
      COMMAND git rev-parse --abbrev-ref HEAD
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE GIT_BRANCH
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
      COMMAND git log -1 --format=%h
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE GIT_COMMIT_HASH
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(SKVDB_GIT_REVISION "${GIT_COMMIT_HASH}-${GIT_BRANCH}")
endif (GIT_FOUND)

include_directories(${PROJECT_BINARY_DIR}
                    ${PROJECT_SOURCE_DIR})

set(SKVDB_VERSION ${VERSION})
set(SKVDB_REVISION ${SKVDB_GIT_REVISION})

configure_file(Config.hpp.in Config.hpp @ONLY)

file(GLOB_RECURSE SRCS *.cpp)
file(GLOB_RECURSE HDRS *.hpp)

add_definitions (-D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -D_REENTRANT)

if (UNIX)
    list(APPEND LIBS -pthread)
    add_definitions(-DBUILDING_UNIX)
elseif(WIN32)
    add_definitions(-DBUILDING_WINDOWS)
endif ()

add_library(${PROJECT_NAME} SHARED
            ${SRCS}
            ${HDRS})

target_link_libraries(${PROJECT_NAME})

install(TARGETS ${PROJECT_NAME} DESTINATION lib)
