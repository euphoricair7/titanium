#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include "titanium/utils/order_generator.hpp"
#include "titanium/engine/baseline_engine.hpp"
#include "titanium/engine/titanium_engine.hpp"

using namespace titanium;

static double ns_to_ms(std::uint64_t ns) {
    return static_cast<double>(ns) / 1'000'000.0;
}

void print_profile(const TitaniumEngine::ProfileStats& p, std::size_t order_count, const char* label) {
    std::cout << "\n[" << label << " Profile]\n";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "  process_order total : " << ns_to_ms(p.process_order_ns) << " ms (" << p.process_order_calls << " calls)\n";
    std::cout << "  match_buy total     : " << ns_to_ms(p.match_buy_ns) << " ms (" << p.match_buy_calls << " calls)\n";
    std::cout << "  match_sell total    : " << ns_to_ms(p.match_sell_ns) << " ms (" << p.match_sell_calls << " calls)\n";
    std::cout << "  add_to_bids total   : " << ns_to_ms(p.add_bid_ns) << " ms (" << p.add_bid_calls << " calls)\n";
    std::cout << "    - bid search      : " << ns_to_ms(p.add_bid_search_ns) << " ms\n";
    std::cout << "    - bid shifts      : " << ns_to_ms(p.add_bid_shift_ns) << " ms\n";
    std::cout << "    - bid tracker map : " << ns_to_ms(p.add_bid_tracker_ns) << " ms\n";
    std::cout << "  add_to_asks total   : " << ns_to_ms(p.add_ask_ns) << " ms (" << p.add_ask_calls << " calls)\n";
    std::cout << "    - ask search      : " << ns_to_ms(p.add_ask_search_ns) << " ms\n";
    std::cout << "    - ask shifts      : " << ns_to_ms(p.add_ask_shift_ns) << " ms\n";
    std::cout << "    - ask tracker map : " << ns_to_ms(p.add_ask_tracker_ns) << " ms\n";
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
    engine.reset_profile_stats();
    auto start = std::chrono::high_resolution_clock::now();
    for (const auto& order : orders) {
        engine.process_order(order);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Titanium (std::array): " << static_cast<long long>(orders.size() / elapsed.count()) << " Ops/Sec" << std::endl;
    const auto& p = engine.profile_stats();
    std::cout << "  Book levels after run: bids=" << engine.get_bid_count() 
              << ", asks=" << engine.get_ask_count()
              << ", tracked orders=" << engine.get_tracker_size() << std::endl;
    print_profile(p, orders.size(), "Titanium");
}

void run_titanium_batched(const std::vector<Order>& orders) {
    TitaniumEngine engine;
    engine.reset_profile_stats();
    auto start = std::chrono::high_resolution_clock::now();
    engine.process_orders_batched(orders.data(), orders.size());
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Titanium + GPU Async (Batched): " << static_cast<long long>(orders.size() / elapsed.count()) << " Ops/Sec" << std::endl;
    const auto& p = engine.profile_stats();
    std::cout << "  Book levels after run: bids=" << engine.get_bid_count() 
              << ", asks=" << engine.get_ask_count()
              << ", tracked orders=" << engine.get_tracker_size() << std::endl;
    print_profile(p, orders.size(), "Titanium+GPU");
}

int main() {
    std::size_t num_orders = 1'000'000;
    auto orders = generate_dummy_orders(num_orders);

    std::cout << "Comparing Engine Performance (1M Orders)..." << std::endl;
    run_baseline(orders);
    run_titanium(orders);
    run_titanium_batched(orders);

    return 0;
}
