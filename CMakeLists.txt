cmake_minimum_required(VERSION 3.29)
project(ysm LANGUAGES CXX)

if (NOT YSM_DEPENDENCY_PROVIDER STREQUAL "conan")
    message(FATAL_ERROR "Please build the project using bootstrap scripts.")
endif ()

include(CTest)
option(YSM_BUILD_BENCHMARKS "Build the ysm microbenchmark executable" OFF)
include(third-party/import.cmake)

option(YSM_ENABLE_DEBUG_LOG
        "Compile Debug and Trace logging into ysm-core and ysm-lib" OFF)

add_subdirectory(modules/core)
add_subdirectory(modules/lib)
if (YSM_BUILD_BENCHMARKS)
    add_subdirectory(modules/benchmark)
endif ()
if (BUILD_TESTING)
    add_subdirectory(modules/test)
endif ()
