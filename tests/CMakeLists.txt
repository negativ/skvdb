cmake_minimum_required(VERSION 2.8.11)

project(skvdb-tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(VERSION "0.0.1")

set (CMAKE_CXX_STANDARD 17)

enable_testing()

if (WIN32)
    configure_file(CMakeListsWIN32.txt.in googletest-download/CMakeLists.txt)

    execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
                    RESULT_VARIABLE result
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )

    if(result)
        message(FATAL_ERROR "CMake step for googletest failed: ${result}")
    endif()

    execute_process(COMMAND ${CMAKE_COMMAND} --build .
                    RESULT_VARIABLE result
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download )

    if(result)
        message(FATAL_ERROR "Build step for googletest failed: ${result}")
    endif()

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                     ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                     EXCLUDE_FROM_ALL)

    list(APPEND LIBS gtest_main)
elseif(UNIX)
    find_package(GTest REQUIRED)

    include(GoogleTest)

    include_directories(${GTEST_INCLUDE_DIR})

    list(APPEND LIBS ${GTEST_LIBRARY} ${GTEST_MAIN_LIBRARY} -pthread)
endif()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../lib/")

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

add_executable(skv-storage-engine-test skv-storage-engine-test.cpp)
target_link_libraries(skv-storage-engine-test ${LIBS} skv)
add_test(skv-storage-engine-test skv-storage-engine-test)

add_executable(skv-property-test skv-property-test.cpp)
target_link_libraries(skv-property-test ${LIBS} skv)
add_test(skv-property-test skv-property-test)

add_executable(skv-entry-test skv-entry-test.cpp)
target_link_libraries(skv-entry-test ${LIBS} skv)
add_test(skv-entry-test skv-entry-test)

add_executable(skv-volume-test skv-volume-test.cpp)
target_link_libraries(skv-volume-test ${LIBS} skv)
add_test(skv-volume-test skv-volume-test)

add_executable(skv-mru-test skv-mru-test.cpp)
target_link_libraries(skv-mru-test ${LIBS} skv)
add_test(skv-mru-test skv-mru-test)

add_executable(skv-vfsstorage-test skv-vfsstorage-test.cpp)
target_link_libraries(skv-vfsstorage-test ${LIBS} skv)
add_test(skv-vfsstorage-test skv-vfsstorage-test)

add_executable(skv-perfomance-test skv-perfomance-test.cpp)
target_link_libraries(skv-perfomance-test ${LIBS} skv)
add_test(skv-zperfomance-test skv-perfomance-test)

add_custom_command(
    TARGET skv-perfomance-test
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:skv>
        $<TARGET_FILE_DIR:skv-perfomance-test>/$<TARGET_FILE_NAME:skv>
)
