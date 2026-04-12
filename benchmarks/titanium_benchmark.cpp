#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <exception>
#include <limits>
#include "titanium/utils/order_generator.hpp"
#include "titanium/engine/baseline_engine.hpp"
#define TITANIUM_ENABLE_PROFILING 1
#include "titanium/engine/titanium_engine.hpp"

using namespace titanium;

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
    std::cout << "Titanium (std::array): " << static_cast<long long>(orders.size() / elapsed.count()) << " Ops/Sec" << std::endl;
    const auto& p = engine.profile_stats();
    std::cout << "  Book levels after run: bids=" << engine.get_bid_count() 
              << ", asks=" << engine.get_ask_count()
              << ", tracked orders=" << engine.get_tracker_size() << std::endl;
    print_profile(p, orders.size(), "Titanium");
}



int main() {
    std::size_t num_orders = 1'000'000;
    auto orders = generate_dummy_orders(num_orders);
    print_order_price_range(orders);

    try {
        std::cout << "Comparing Engine Performance (1M Orders)..." << std::endl;
        std::cout << "[Stage 1] Baseline Standard Engine" << std::endl;
        run_baseline(orders);
        std::cout << "\n[Stage 2] Titanium CPU-Only Engine" << std::endl;
        run_titanium(orders);
    } catch (const std::exception& e) {
        std::cerr << "Benchmark failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
