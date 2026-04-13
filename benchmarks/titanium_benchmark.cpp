#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <string>
#include <map>
#include <algorithm>
#include <exception>
#include <limits>
#include <cstring>

#include "titanium/utils/order_generator.hpp"
#include "titanium/engine/baseline_engine.hpp"

// Disable fine-grained profiling for accurate throughput measurement
#define TITANIUM_ENABLE_PROFILING 0
#include "titanium/engine/titanium_engine.hpp"

#include "titanium/engine/cpu/risk_kernel.hpp"
#include "titanium/engine/cuda/risk_kernel.cuh"

#include <thread>
#include <atomic>
#include <future>

using namespace titanium;

static constexpr std::size_t RISK_BATCH_SIZE = 16384;
static constexpr std::size_t ENGINE_BATCH_SIZE = 65536;

/**
 * @brief Helper class to execute CPU risk checks across all available cores.
 */
class MultiThreadedRiskEngine {
public:
    MultiThreadedRiskEngine(std::size_t batch_size) : batch_size_(batch_size) {
        num_threads_ = std::thread::hardware_concurrency();
        if (num_threads_ < 1) num_threads_ = 1;
    }

    void submit_batch(const Order* orders, float* results, std::size_t count) {
        std::size_t chunk = (count + num_threads_ - 1) / num_threads_;
        std::vector<std::future<void>> futures;

        for (std::size_t i = 0; i < num_threads_; ++i) {
            std::size_t start = i * chunk;
            if (start >= count) break;
            std::size_t end = std::min(start + chunk, count);

            futures.push_back(std::async(std::launch::async, [=]() {
                run_risk_check_cpu(orders + start, end - start, results + start);
            }));
        }

        for (auto& f : futures) f.get();
    }

    void synchronize() {
        // CPU implementation is synchronous within submit_batch for each batch
    }

private:
    std::size_t batch_size_;
    std::size_t num_threads_;
};

struct BenchResult {
    long long ops_per_sec;
    double ns_per_order;
    std::string label;
};

static double ns_to_ms(std::uint64_t ns) {
    return static_cast<double>(ns) / 1000000.0;
}

static void print_order_price_range(const std::vector<Order>& orders) {
    uint32_t min_price = std::numeric_limits<uint32_t>::max();
    uint32_t max_price = 0;
    for (const auto& order : orders) {
        if (order.price < min_price) min_price = order.price;
        if (order.price > max_price) max_price = order.price;
    }
    std::cout << "Order price range: " << min_price << " - " << max_price << std::endl;
}

static void seed_hot_window(TitaniumEngine& engine) {
    for (std::size_t i = 0; i < 100; ++i) {
        const uint32_t price = 9900u + static_cast<uint32_t>((i * 200u) / 99u);
        Order placeholder{};
        placeholder.id = 1'000'000u + static_cast<uint64_t>(i);
        placeholder.timestamp = static_cast<uint64_t>(i);
        placeholder.next = 0;
        placeholder.price = price;
        placeholder.quantity = 1;
        placeholder.side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        placeholder.type = OrderType::Limit;
        engine.process_order(placeholder);
    }
}

void print_profile(const TitaniumEngine::ProfileStats& p, std::size_t order_count, const char* label) {
    if (p.process_order_calls == 0) return;
    std::cout << "\n[" << label << " Profile]\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  process_order total : " << ns_to_ms(p.process_order_ns) << " ms (" << p.process_order_calls << " calls)\n";
    std::cout << "  match_buy total     : " << ns_to_ms(p.match_buy_ns) << " ms (" << p.match_buy_calls << " calls)\n";
    std::cout << "  match_sell total    : " << ns_to_ms(p.match_sell_ns) << " ms (" << p.match_sell_calls << " calls)\n";
    std::cout << "  add_to_bids total   : " << ns_to_ms(p.add_bid_ns) << " ms (" << p.add_bid_calls << " calls)\n";
    std::cout << "  add_to_asks total   : " << ns_to_ms(p.add_ask_ns) << " ms (" << p.add_ask_calls << " calls)\n";
    if (p.batched_calls > 0) {
        std::cout << "  batched total       : " << ns_to_ms(p.batched_total_ns) << " ms (" << p.batched_orders << " orders)\n";
        std::cout << "  batched memcpy      : " << ns_to_ms(p.batched_memcpy_ns) << " ms\n";
        std::cout << "  batched sync        : " << ns_to_ms(p.batched_sync_ns) << " ms\n";
        std::cout << "  batched submit      : " << ns_to_ms(p.batched_submit_ns) << " ms\n";
        std::cout << "  batched CPU process : " << ns_to_ms(p.batched_cpu_process_ns) << " ms\n";
    }
    std::cout << "  input orders        : " << order_count << "\n";
}

BenchResult run_baseline(const std::vector<Order>& orders) {
    BaselineEngine engine;
    auto start = std::chrono::high_resolution_clock::now();
    for (const auto& order : orders) {
        engine.process_order(order);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    long long ops = static_cast<long long>(orders.size() / elapsed.count());
    double ns = (elapsed.count() * 1e9) / static_cast<double>(orders.size());
    std::cout << "Baseline (std::map) : " << ops << " Ops/Sec" << std::endl;
    return {ops, ns, "Baseline (std::map)"};
}

BenchResult run_titanium(const std::vector<Order>& orders) {
    TitaniumEngine engine;
    seed_hot_window(engine);
    engine.reset_profile_stats();
    auto start = std::chrono::high_resolution_clock::now();
    std::size_t processed = 0;
    for (const auto& order : orders) {
        engine.process_order(order);
        ++processed;
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    double ns_per_order  = (elapsed.count() * 1e9) / static_cast<double>(orders.size());
    long long ops_per_sec = static_cast<long long>(orders.size() / elapsed.count());
    std::cout << "Titanium CPU-Only: " << ops_per_sec << " Ops/Sec" << std::endl;
    return {ops_per_sec, ns_per_order, "Titanium CPU-Only"};
}

// NEW: CPU does EVERYTHING — matching + full risk math — on every order, sequentially.
// This is what a traditional exchange without GPU offloading would look like.
BenchResult run_titanium_cpu_full(const std::vector<Order>& orders) {
    TitaniumEngine engine;
    seed_hot_window(engine);
    engine.reset_profile_stats();

    std::vector<float> risk_result(1);

    auto start = std::chrono::high_resolution_clock::now();
    for (const auto& order : orders) {
        run_risk_check_cpu(&order, 1, risk_result.data());
        if (risk_result[0] >= 0.0f) {
            engine.process_order(order);
        }
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;
    double ns_per_order = (elapsed.count() * 1e9) / static_cast<double>(orders.size());
    long long ops_per_sec = static_cast<long long>(orders.size() / elapsed.count());
    std::cout << "Titanium CPU-Full: " << ops_per_sec << " Ops/Sec" << std::endl;
    return {ops_per_sec, ns_per_order, "Titanium CPU-Full"};
}

// NEW: CPU version of the Batched Pipeline.
// Main thread matches orders speculatively, while a separate 'cpu_worker' thread
// uses ALL other CPU cores to perform the math in parallel.
BenchResult run_titanium_cpu_multithreaded(const std::vector<Order>& orders) {
    TitaniumEngine engine;
    seed_hot_window(engine);
    engine.reset_profile_stats();

    std::size_t total_count = orders.size();
    std::vector<float> results(total_count);
    MultiThreadedRiskEngine risk_engine(ENGINE_BATCH_SIZE);

    auto start = std::chrono::high_resolution_clock::now();

    std::thread cpu_worker([&]() {
        std::size_t offset = 0;
        while (offset < total_count) {
            std::size_t current_batch = std::min(ENGINE_BATCH_SIZE, total_count - offset);
            risk_engine.submit_batch(orders.data() + offset, results.data() + offset, current_batch);
            for (std::size_t j = 0; j < current_batch; ++j) {
                if (results[offset + j] < 0.0f) {
                    while (!engine.push_cancel_id(orders[offset + j].id)) {}
                }
            }
            offset += current_batch;
        }
    });

    for (std::size_t i = 0; i < total_count; ++i) {
        engine.process_order(orders[i]);
        engine.drain_cancellations();
    }

    cpu_worker.join();
    engine.drain_cancellations();
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;
    double ns_per_order = (elapsed.count() * 1e9) / static_cast<double>(total_count);
    long long ops_per_sec = static_cast<long long>(total_count / elapsed.count());
    std::cout << "Titanium CPU-Multi: " << ops_per_sec << " Ops/Sec" << std::endl;
    return {ops_per_sec, ns_per_order, "Titanium CPU-Multi"};
}

BenchResult run_titanium_batched(const std::vector<Order>& orders) {
    TitaniumEngine engine;
    seed_hot_window(engine);
    engine.reset_profile_stats();
    auto start = std::chrono::high_resolution_clock::now();
    engine.process_orders_batched(orders.data(), orders.size());
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    double ns_per_order  = (elapsed.count() * 1e9) / static_cast<double>(orders.size());
    long long ops_per_sec = static_cast<long long>(orders.size() / elapsed.count());
    std::cout << "Titanium Batched (GPU): " << ops_per_sec << " Ops/Sec" << std::endl;
    return {ops_per_sec, ns_per_order, "Titanium Batched (GPU)"};
}

BenchResult run_cpu_risk_benchmark(const std::vector<Order>& orders) {
    std::vector<float> results(orders.size());
    auto start = std::chrono::high_resolution_clock::now();
    
    std::size_t offset = 0;
    while (offset < orders.size()) {
        std::size_t count = std::min(RISK_BATCH_SIZE, orders.size() - offset);
        run_risk_check_cpu(orders.data() + offset, count, results.data() + offset);
        offset += count;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    long long ops = static_cast<long long>(orders.size() / elapsed.count());
    double ns = (elapsed.count() * 1000.0) / static_cast<double>(orders.size()); // Actually unused for risk throughput
    std::cout << "CPU Risk Throughput (Serial): " << ops << std::endl;
    return {ops, ns, "CPU Risk (Serial)"};
}

BenchResult run_cpu_risk_multithreaded_benchmark(const std::vector<Order>& orders) {
    std::vector<float> results(orders.size());
    MultiThreadedRiskEngine engine(RISK_BATCH_SIZE);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    std::size_t offset = 0;
    while (offset < orders.size()) {
        std::size_t count = std::min(RISK_BATCH_SIZE, orders.size() - offset);
        engine.submit_batch(orders.data() + offset, results.data() + offset, count);
        offset += count;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    long long ops = static_cast<long long>(orders.size() / elapsed.count());
    std::cout << "CPU Risk Throughput (Parallel): " << ops << std::endl;
    return {ops, 0.0, "CPU Risk (Parallel)"};
}

BenchResult run_gpu_risk_benchmark(const std::vector<Order>& orders) {
    Order* pinned_orders = alloc_pinned_orders(orders.size());
    float* pinned_results = alloc_pinned_results(orders.size());
    std::memcpy(pinned_orders, orders.data(), orders.size() * sizeof(Order));

    AsyncRiskEngine gpu_engine(RISK_BATCH_SIZE);
    auto start = std::chrono::high_resolution_clock::now();
    
    std::size_t offset = 0;
    while (offset < orders.size()) {
        std::size_t count = std::min(RISK_BATCH_SIZE, orders.size() - offset);
        gpu_engine.submit_batch(pinned_orders + offset, pinned_results + offset, count);
        offset += count;
    }
    gpu_engine.synchronize();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    long long ops = static_cast<long long>(orders.size() / elapsed.count());
    std::cout << "GPU Risk Throughput (Batched): " << ops << std::endl;

    free_pinned_orders(pinned_orders);
    free_pinned_results(pinned_results);
    return {ops, 0.0, "GPU Risk (Parallel)"};
}

int main() {
    std::vector<std::size_t> sizes = { 1'000'000, 5'000'000, 10'000'000, 15'000'000, 20'000'000 };
    std::map<std::size_t, std::vector<BenchResult>> all_results;

    for (auto n : sizes) {
        std::cout << "\n============================================\n";
        std::cout << "  RUNNING BENCHMARK FOR " << n / 1000000 << "M ORDERS\n";
        std::cout << "============================================\n";
        
        auto orders = generate_dummy_orders(n);
        std::vector<BenchResult> results;

        try {
            results.push_back(run_baseline(orders));
            results.push_back(run_titanium(orders));
            results.push_back(run_titanium_cpu_full(orders));
            results.push_back(run_titanium_cpu_multithreaded(orders));
            results.push_back(run_titanium_batched(orders));
            results.push_back(run_cpu_risk_benchmark(orders));
            results.push_back(run_cpu_risk_multithreaded_benchmark(orders));
            results.push_back(run_gpu_risk_benchmark(orders));
            
            all_results[n] = results;
        } catch (const std::exception& e) {
            std::cerr << "Benchmark failed for " << n << ": " << e.what() << std::endl;
        }
    }

    // Final Comparison Table
    std::cout << "\n\nFINAL PERFORMANCE COMPARISON TABLE (Ops/Sec)\n";
    std::cout << "| Scale | Baseline | Titanium | CPU Serial | CPU Multi | CPU + GPU |\n";
    std::cout << "| :--- | :--- | :--- | :--- | :--- | :--- |\n";
    
    for (auto n : sizes) {
        if (all_results.find(n) == all_results.end()) continue;
        const auto& r = all_results[n];
        std::cout << "| " << n / 1000000 << "M | "
                  << r[0].ops_per_sec << " | "
                  << r[1].ops_per_sec << " | "
                  << r[2].ops_per_sec << " | "
                  << r[3].ops_per_sec << " | "
                  << r[4].ops_per_sec << " |\n";
    }

    std::cout << "\nLATENCY COMPARISON (Avg ns/order)\n";
    std::cout << "| Scale | Titanium (Unsafe) | CPU Serial | CPU Multi | CPU + GPU |\n";
    std::cout << "| :--- | :--- | :--- | :--- | :--- |\n";
    for (auto n : sizes) {
        if (all_results.find(n) == all_results.end()) continue;
        const auto& r = all_results[n];
        std::cout << "| " << n / 1000000 << "M | "
                  << std::fixed << std::setprecision(1)
                  << r[1].ns_per_order << " | "
                  << r[2].ns_per_order << " | "
                  << r[3].ns_per_order << " | "
                  << r[4].ns_per_order << " |\n";
    }

    return 0;
}