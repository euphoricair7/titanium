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

# Includes
include_directories(include)

# Core Library
add_library(titanium_core STATIC
    src/engine/baseline_engine.cpp
    src/engine/titanium_engine.cpp
    src/utils/order_generator.cpp
)

target_include_directories(titanium_core PUBLIC include)

# Google Benchmark
include(FetchContent)
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG main
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

# Benchmarks
add_executable(titanium_bench_baseline benchmarks/baseline_benchmark.cpp)
target_link_libraries(titanium_bench_baseline PRIVATE titanium_core)

add_executable(titanium_bench_dummy benchmarks/dummy_bench.cpp)
target_link_libraries(titanium_bench_dummy PRIVATE titanium_core benchmark::benchmark_main pthread)

add_executable(titanium_bench_optimized benchmarks/titanium_benchmark.cpp)
target_link_libraries(titanium_bench_optimized PRIVATE titanium_core)

# Unit Tests
add_executable(titanium_test tests/test.cpp)
target_link_libraries(titanium_test PRIVATE titanium_core)

add_executable(titanium_stress_spsc tests/stress_test_spsc.cpp)
target_link_libraries(titanium_stress_spsc PRIVATE titanium_core pthread)
