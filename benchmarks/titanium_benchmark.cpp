#include <iostream>
#include <chrono>
#include <vector>
#include "titanium/utils/order_generator.hpp"
#include "titanium/engine/baseline_engine.hpp"
#include "titanium/engine/titanium_engine.hpp"

using namespace titanium;

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
    auto start = std::chrono::high_resolution_clock::now();
    for (const auto& order : orders) {
        engine.process_order(order);
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Titanium (std::array): " << static_cast<long long>(orders.size() / elapsed.count()) << " Ops/Sec" << std::endl;
}

void run_titanium_batched(const std::vector<Order>& orders) {
    TitaniumEngine engine;
    auto start = std::chrono::high_resolution_clock::now();
    engine.process_orders_batched(orders.data(), orders.size());
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Titanium + GPU Async (Batched): " << static_cast<long long>(orders.size() / elapsed.count()) << " Ops/Sec" << std::endl;
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
