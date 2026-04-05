cmake_minimum_required(VERSION 3.20)
project(titanium LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)
set(CMAKE_CUDA_ARCHITECTURES 86) # RTX 3070 Ti is Ampere (Compute 8.6)

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
    src/engine/risk_kernel.cu
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

add_executable(titanium_bench_optimized benchmarks/titanium_benchmark.cpp)
target_link_libraries(titanium_bench_optimized PRIVATE titanium_core)

# Unit Tests
add_executable(titanium_test tests/test.cpp)
target_link_libraries(titanium_test PRIVATE titanium_core)

find_package(Threads REQUIRED)

add_executable(titanium_stress_spsc tests/stress_test_spsc.cpp)
target_link_libraries(titanium_stress_spsc PRIVATE titanium_core Threads::Threads)

add_executable(titanium_test_cuda tests/test_cuda.cpp)
target_link_libraries(titanium_test_cuda PRIVATE titanium_core)
