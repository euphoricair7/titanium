# Titanium Trading Engine

Titanium is a high-performance, heterogeneous trading engine framework designed for ultra-low latency execution. It leverages **C++20 AVX2 SIMD** for the matching logic and **NVIDIA CUDA** for asynchronous, high-intensity risk management.

## HPC Architecture

The engine is built on four core high-performance pillars:
1.  **L1 Cache Locality**: Uses a **Structure-of-Arrays (SoA)** memory layout allowing the CPU to scan 8 price levels per clock cycle.
2.  **Lock-Free Pipeline**: A single-producer, single-consumer (SPSC) ring buffer manages communication between threads without kernel-level mutex stalls.
3.  **Heterogeneous Decoupling**: The engine spawns a background thread to manage GPU logistics, allowing the main Matching Engine to run at 100% throughput.
4.  **Zero-Copy GPU Transfer**: Uses `cudaHostRegister` to map system RAM directly to the GPU via DMA, eliminating the overhead of standard `memcpy`.

## Prerequisites

### Hardware
- **CPU**: x86_64 with AVX2 support.
- **GPU**: NVIDIA GPU (Ampere architecture or newer recommended, e.g., RTX 30-series).

### Software
- **OS**: Windows 10/11 (with MSVC compiler).
- **Toolchain**: Visual Studio 2022 (with "Desktop development with C++").
- **CUDA**: NVIDIA CUDA Toolkit v12.x or 13.x.
- **Build System**: CMake 3.20+ and Ninja.

## Build Instructions (Windows)

To build the engine with full optimizations (Release mode), follow these steps:

1.  **Open PowerShell**.
2.  **Initialize the Environment**:
    ```powershell
    cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 && powershell'
    ```
3.  **Configure & Build**:
    ```powershell
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ```

## Performance Benchmarking

To run the apples-to-apples comparison between the CPU-only engine and the Asynchronous GPU pipeline:

```powershell
.\build\titanium_bench_optimized.exe
```

### Expected Results
Under a heavy risk load (50-iteration Monte Carlo simulation):
-   **Titanium CPU**: ~1.0 Million Ops/Sec
-   **Titanium GPU Async**: **~4.5+ Million Ops/Sec**

## Testing & Verification

Ensure the engine is functioning correctly by running the automated test suite:

-   **Logic Test**: `.\build\titanium_test.exe` (Verifies matching logic correctness).
*   **GPU Accuracy**: `.\build\titanium_test_cuda.exe` (Verifies GPU math against CPU).
*   **Stress Test**: `.\build\titanium_stress_spsc.exe` (Tests 10M-order lock-free integrity).

## License

This project is licensed under the MIT License.
