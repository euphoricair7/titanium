#include <benchmark/benchmark.h>

// Benchmark function
static void BM_Dummy(benchmark::State& state) {
    for (auto _ : state) {
        int x = 0;
        x += 42;
        benchmark::DoNotOptimize(x);
    }
}

// Register benchmark
BENCHMARK(BM_Dummy);

// Main function (provided by benchmark)
BENCHMARK_MAIN();