#include <benchmark/benchmark.h>
#include <vector>
#include <chrono>
#include "titanium/utils/order_generator.hpp"
#include "titanium/engine/titanium_engine.hpp"

using namespace titanium;

// CPU strictly runs the heavy math baseline before matching
static void BM_Titanium_CPU_Heavy(benchmark::State& state) {
    auto orders = generate_dummy_orders(state.range(0));
    
    for (auto _ : state) {
        TitaniumEngine engine;
        auto start = std::chrono::high_resolution_clock::now();
        
        for (const auto& order : orders) {
            engine.process_order_heavy_cpu(order);
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
    }
    
    state.counters["Ops/Sec"] = benchmark::Counter(state.iterations() * state.range(0), benchmark::Counter::kIsRate);
}

// GPU Async runs via the heavily pipelined decoupled zero-copy streams
static void BM_Titanium_GPU_Async_Heavy(benchmark::State& state) {
    auto orders = generate_dummy_orders(state.range(0));
    
    for (auto _ : state) {
        TitaniumEngine engine;
        auto start = std::chrono::high_resolution_clock::now();
        
        engine.process_orders_batched(orders.data(), orders.size());
        
        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed_seconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
    }
    
    state.counters["Ops/Sec"] = benchmark::Counter(state.iterations() * state.range(0), benchmark::Counter::kIsRate);
}

BENCHMARK(BM_Titanium_CPU_Heavy)
    ->Arg(100'000)
    ->Arg(1'000'000)
    ->Arg(5'000'000)
    ->UseManualTime()
    ->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Titanium_GPU_Async_Heavy)
    ->Arg(100'000)
    ->Arg(1'000'000)
    ->Arg(5'000'000)
    ->UseManualTime()
    ->Unit(benchmark::kMillisecond);

BENCHMARK_MAIN();
