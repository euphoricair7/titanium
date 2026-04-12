# Codebase Analysis: Titanium Fast Engine Comparison

This report details a side-by-side comparison between the original project version (`HP_PROJ`) and the updated version (`HP_PROJ_ANALYZE`), examining their system architecture, performance profiles, and uncovering the reason behind the drastically different benchmark outcomes.

## 1. Core Architecture Changes and Matching Engine

The most massive change between the two versions lies in how the order books (bids and asks arrays) are built, representing a leap from $O(N)$ insertion times to $O(1)$ operations natively accelerated by CPU hardware routines.

| Feature | Old Version (`HP_PROJ`) | New Version (`HP_PROJ_ANALYZE`) |
| :--- | :--- | :--- |
| **Order Book Structure** | Dynamic sorted arrays (`std::memmove`) | Fixed-Window O(1) Arrays (`std::bitset`) |
| **Insertion Time** | O(N) memory shifting | O(1) masked indexing |
| **Price Search** | 256-bit SIMD scanning & residual loops | Instant Bitwise lookups (`_BitScanReverse64`) |
| **Profiling Strategy** | Hardcoded | Configurable via CMake / Preprocessor Definitions |

**Detailed Overview:**
- **The Old Variant** handled price levels by physically copying and dragging memory around to insert new numbers using `std::memmove(&prices[i+1], &prices[i], ...)`. It tried to speed this up with vectorized AVX SIMD instructions (`_mm256_load_si256`) to search for arrays, but at its heart, it was severely limited by memory shuffling overhead.
- **The New Variant** ditched dynamically sized arrays and introduced a fixed-window model based on bitmasks. Each price window leverages a `std::bitset` mask. To find the "highest" or "lowest" bid/ask, the new version delegates natively to ultra-fast hardware intrinsics like `_BitScanForward64` and `_BitScanReverse64` natively mapping to machine instructions `bsf`/`bsr` or `lzcnt` to identify active memory blocks instantly without loops.

## 2. Order Processing Pipeline

The benchmarking targets shifted entirely between the two projects, masking standard matching overhead vs. mathematical risk computation.

| Feature | Old Version (`HP_PROJ`) | New Version (`HP_PROJ_ANALYZE`) |
| :--- | :--- | :--- |
| **Benchmark Targets** | Math-included Single-CPU vs Batched GPU Engine | Decoupled Matching Engine vs Separate Async Risk tests |
| **Engine Testing** | Forced an intentional synchronous heavy math loop before moving to `process_order` in `process_order_heavy_cpu`. | Benchmarks raw CPU matching throughput natively (No math delays) via simple `process_order`. |

**Detailed Overview:**
- **Old Project**: In `BM_Titanium_CPU_Heavy`, the code explicitly chained a 50-iterate math loop measuring trigonometric bounds BEFORE registering the CPU order in order to simulate standard heavy limits. The alternative test `BM_Titanium_GPU_Async_Heavy` effectively batched and shipped those checks to the CUDA kernel in the background thread via SPSC thread-safe queue streams, validating it correctly.
- **New Project**: The new algorithm measures the direct CPU order matching mechanics in "Titanium CPU-Only" running directly in a single thread sequentially, separating risk modeling altogether.

## 3. Discrepancy Breakdown: The "Cheated" Risk Kernel Benchmarks

The most alarming difference is in the calculated "Risk Checks/Sec". In the raw execution tests of the new codebase, the CPU Risk Throughput reported `~233.07 Million Checks/Sec`, while the massively parallel CUDA GPU model staggered at only `~52.8 Million Checks/Sec`.

This severe discrepancy points to **algorithmic divergence (cheating)** rather than hardware dominance. Upon checking `include/titanium/engine/cpu/risk_kernel.hpp` and `src/engine/cuda/risk_kernel.cu` natively in the New Version, we uncovered fundamental differences in test subjects:

- **GPU Kernel (Codebase B Model)**: The GPU calculates the *actual* dummy simulation by operating a massive 50-step iterative block combining $sin(p)$, $cos(v)$, and $exp(v)$ for every order.
- **CPU Kernel (New Update)**: The new CPU risk engine's method `run_risk_check_cpu()` abandons trigonometric simulations completely. It runs a single Boolean branching rule: 
  ```cpp
  if (o.price > 1000000 || o.quantity > 500000) { results[i] = -1.0f; }
  ```
The new benchmark measures a single cache-efficient boolean `IF` statement on the CPU against a 50-length trigonometric mathematical array simulation on the GPU, generating the artificial conclusion that "the CPU outperformed the GPU."

## 4. Hardware Verification Benchmarks

After independently compiling and deploying both builds via `CMake` using Google Benchmark and Custom Scripts, the performance is mapped out as follows:

| Test Configuration | Old Version (`HP_PROJ`) Speed | New Version (`HP_PROJ_ANALYZE`) Speed |
| :--- | :--- | :--- |
| **Baseline (std::map / Mutex)** | ~5.15 Million Ops/Sec | ~7.65 Million Ops/Sec |
| **CPU Engine Match (Heavy Math)** | ~851 k Ops/Sec | **N/A** (Test omitted) |
| **GPU Async Batch (Heavy Math)** | ~3.67 Million Ops/Sec | **N/A** (Test omitted) |
| **Pure Engine Match (No Math)** | **N/A** (Test omitted) | ~16.15 Million Ops/Sec |
| **CPU Risk Only (Branching Check)** | **N/A** | ~233.07 Million Ops/Sec (Cheated) |
| **GPU Risk Only (Trig Check)** | **N/A** | ~52.86 Million Ops/Sec |
