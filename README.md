# Titanium Trading Engine

Titanium is a high-performance, low-latency trading engine framework written in C++20.

## Project Structure

The project follows a standard C++ layout for clarity and maintainability:

- `include/titanium/`: Public headers.
  - `core/`: Core data structures (e.g., `Order`).
  - `engine/`: Matching engine implementations (e.g., `BaselineEngine`).
  - `utils/`: Helper utilities (e.g., `OrderGenerator`).
- `src/`: Implementation files.
- `benchmarks/`: Performance testing suites.
- `tests/`: Unit tests and functional verification.
- `docs/`: Additional documentation and design notes.

## Key Components

### 1. Order Structure (`order.hpp`)
The `Order` struct is designed for high performance:
- **Cache-line Aligned**: Exactly 64 bytes to prevent false sharing and optimize cache usage.
- **POD-Compatible**: Easy to manage in contiguous memory (vectors/pools).

### 2. Matching Engine (`baseline_engine.hpp`)
The current version includes a `BaselineEngine` which:
- Uses a global `Spinlock` for thread safety.
- Implements a Price-Time Priority matching algorithm using `std::map` and `std::vector`.
- Optimized with a `pause` instruction in the spinlock for reduced CPU power consumption during contention.

## Getting Started

### Prerequisites
- CMake 3.20+
- C++20 compatible compiler (GCC 11+, Clang 13+)

### Build Instructions
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

### Running Tests
```bash
./build/titanium_test
```

### Running Benchmarks
To evaluate the performance of the engine, you can run the following targets:

1. **Optimized Benchmark**:
   ```powershell
   # Windows
   .\build\Release\titanium_bench_optimized.exe
   
   # Linux
   ./build/titanium_bench_optimized
   ```
   This benchmark tests the Array-based engine with CUDA-accelerated risk checks.

2. **Baseline Benchmark**:
   ```powershell
   # Windows
   .\build\Release\titanium_bench_baseline.exe
   
   # Linux
   ./build/titanium_bench_baseline
   ```
   This benchmark compares the performance against a standard `std::map`-based engine.

## Testing Suite

The project includes a comprehensive test suite to ensure both functional correctness and performance integrity.

### 1. Functional Tests (`titanium_test`)
Located in [tests/test.cpp](file:///home/grass/Documents/projects/titanium/tests/test.cpp), this suite verifies:
- **Data Layout**: Ensures `Order` is exactly 64 bytes (cache-line aligned).
- **Matching Logic**: Validates perfect matches between Buy and Sell orders.
- **Partial Fills**: Ensures the engine correctly handles leftover quantities in the book.

### 2. Concurrency Stress Test (`titanium_stress_spsc`)
Located in [tests/stress_test_spsc.cpp](file:///home/grass/Documents/projects/titanium/tests/stress_test_spsc.cpp), this test:
- Processes **10 million orders** through the lock-free SPSC queue.
- Verifies zero data loss across threads.

### 3. Performance Benchmarks (`titanium_bench_optimized`)
Located in [benchmarks/titanium_benchmark.cpp](file:///home/grass/Documents/projects/titanium/benchmarks/titanium_benchmark.cpp), this tool:
- Compares the **Baseline Engine** (Map-based) vs **Titanium Engine** (Array-based).
- Verifies that the optimized engine maintains its **~14M Ops/Sec** target.

## Performance
The baseline engine targets ~5-7 million operations per second, while the optimized Titanium engine achieves ~14-16 million operations per second on modern hardware.
