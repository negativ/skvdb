cmake_minimum_required(VERSION 2.8)

project(skvdb-tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(VERSION "0.0.1")


set (CMAKE_CXX_STANDARD 17)

enable_testing()

find_package(GTest REQUIRED)

include(GoogleTest)

include_directories(${GTEST_INCLUDE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/../lib/")

if (UNIX)
    list(APPEND LIBS ${GTEST_LIBRARY} ${GTEST_MAIN_LIBRARY} -pthread)
endif ()

list(APPEND LIBS ${Boost_LIBRARIES})

add_executable(skv-spinlock-test skv-spinlock-test.cpp)
target_link_libraries(skv-spinlock-test ${LIBS} skv)
add_test(skv-spinlock-test skv-spinlock-test)

add_executable(skv-logdevice-test skv-logdevice-test.cpp)
target_link_libraries(skv-logdevice-test ${LIBS} skv)
add_test(skv-logdevice-test skv-logdevice-test)

add_executable(skv-indexrecord-test skv-indexrecord-test.cpp)
target_link_libraries(skv-indexrecord-test ${LIBS} skv)
add_test(skv-indexrecord-test skv-indexrecord-test)

add_executable(skv-indextable-test skv-indextable-test.cpp)
target_link_libraries(skv-indextable-test ${LIBS} skv)
add_test(skv-indextable-test skv-indextable-test)
