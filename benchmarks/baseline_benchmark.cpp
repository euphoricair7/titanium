#include <iostream>
#include <chrono>
#include <vector>
#include "titanium/utils/order_generator.hpp"
#include "titanium/engine/baseline_engine.hpp"

using namespace titanium;

int main() {
    std::cout << "[Titanium - Baseline] Generating 1 million orders for benchmark..." << std::endl;
    std::size_t num_orders = 1'000'000;
    auto orders = generate_dummy_orders(num_orders);

    BaselineEngine engine;

    std::cout << "[Running] Feeding orders into Mutex Baseline Engine..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    for (const auto& order : orders) {
        engine.process_order(order);
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    double ops_per_sec = num_orders / elapsed.count();

    std::cout << "\n==================================================\n";
    std::cout << " BENCHMARK RESULTS (Mutex-Based Baseline Engine)  \n";
    std::cout << "==================================================\n";
    std::cout << " Throughput       : " << static_cast<long long>(ops_per_sec) << " Ops/Sec\n";
    std::cout << "==================================================\n";
    return 0;
}
