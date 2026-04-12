#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
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

using namespace titanium;

static constexpr std::size_t RISK_BATCH_SIZE = 16384;

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

void run_baseline(const std::vector<Order>& orders) {
    BaselineEngine engine;
    auto start = std::chrono::high_resolution_clock::now();
    for (const auto& order : orders) {
        engine.process_order(order);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Baseline (std::map) : " << static_cast<long long>(orders.size() / elapsed.count()) << " Ops/Sec" << std::endl;
}

void run_titanium(const std::vector<Order>& orders) {
    TitaniumEngine engine;
    std::cout << "[Titanium] seeding hot window" << std::endl;
    seed_hot_window(engine);
    std::cout << "[Titanium] seeding complete" << std::endl;
    engine.reset_profile_stats();
    auto start = std::chrono::high_resolution_clock::now();
    std::size_t processed = 0;
    for (const auto& order : orders) {
        engine.process_order(order);
        ++processed;
        if ((processed % 100000) == 0) {
            std::cout << "[Titanium] processed " << processed << " orders" << std::endl;
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Titanium CPU-Only (single order): " << static_cast<long long>(orders.size() / elapsed.count()) << " Ops/Sec" << std::endl;
    const auto& p = engine.profile_stats();
    std::cout << "  Book levels after run: bids=" << engine.get_bid_count() 
              << ", asks=" << engine.get_ask_count()
              << ", tracked orders=" << engine.get_tracker_size() << std::endl;
    print_profile(p, orders.size(), "Titanium");
}

void run_titanium_batched(const std::vector<Order>& orders) {
    TitaniumEngine engine;
    std::cout << "[Titanium Batched] seeding hot window" << std::endl;
    seed_hot_window(engine);
    std::cout << "[Titanium Batched] seeding complete" << std::endl;
    engine.reset_profile_stats();
    auto start = std::chrono::high_resolution_clock::now();
    engine.process_orders_batched(orders.data(), orders.size());
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Titanium Batched (speculative + GPU): " << static_cast<long long>(orders.size() / elapsed.count()) << " Ops/Sec" << std::endl;
    const auto& p = engine.profile_stats();
    std::cout << "  Book levels after run: bids=" << engine.get_bid_count() 
              << ", asks=" << engine.get_ask_count()
              << ", tracked orders=" << engine.get_tracker_size() << std::endl;
    print_profile(p, orders.size(), "Titanium Batched");
}

void run_cpu_risk_benchmark(const std::vector<Order>& orders) {
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
    std::cout << "CPU Risk Throughput (Batched)  : " << static_cast<long long>(orders.size() / elapsed.count()) << " RiskChecks/Sec" << std::endl;
}

void run_gpu_risk_benchmark(const std::vector<Order>& orders) {
    // Allocate pinned memory for max performance
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
    // Single synchronisation at the end to measure pipeline throughput
    gpu_engine.synchronize();
    
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "GPU Risk Throughput (Batched)  : " << static_cast<long long>(orders.size() / elapsed.count()) << " RiskChecks/Sec" << std::endl;

    free_pinned_orders(pinned_orders);
    free_pinned_results(pinned_results);
}

int main() {
    std::size_t num_orders = 1'000'000;
    auto orders = generate_dummy_orders(num_orders);
    print_order_price_range(orders);

    try {
        std::cout << "Comparing Engine Performance (1M Orders)..." << std::endl;
        std::cout << "[Stage 1] Baseline Standard Engine" << std::endl;
        run_baseline(orders);
        
        std::cout << "\n[Stage 2] Titanium CPU-Only (Single Order Processing)" << std::endl;
        run_titanium(orders);
        
        std::cout << "\n[Stage 3] Titanium Batched (Speculative + GPU Offload)" << std::endl;
        run_titanium_batched(orders);
        
        std::cout << "\n[Stage 4] CPU Risk Kernel Throughput" << std::endl;
        run_cpu_risk_benchmark(orders);
        
        std::cout << "\n[Stage 5] GPU Risk Kernel Throughput (CUDA)" << std::endl;
        run_gpu_risk_benchmark(orders);
        
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}