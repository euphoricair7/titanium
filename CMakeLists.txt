cmake_minimum_required(VERSION 3.20)
project(titanium LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -O3
        -march=native
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

include(FetchContent)


FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG main
)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(CMAKE_BUILD_TYPE Release)
FetchContent_MakeAvailable(benchmark)

add_executable(titanium_bench
    benchmarks/dummy_bench.cpp
)

target_link_libraries(titanium_bench
    benchmark::benchmark_main
    pthread
)

target_include_directories(titanium_bench PRIVATE include)
