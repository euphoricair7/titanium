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
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Running Tests
```bash
./build/titanium_test
```

### Running Benchmarks
```bash
./build/titanium_bench_baseline
```

## Performance
The baseline engine targets ~5-7 million operations per second on modern hardware using the provided `baseline_benchmark`.
