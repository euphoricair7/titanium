#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>
#include "titanium/utils/order_generator.hpp"
#include "titanium/engine/cuda/risk_kernel.cuh"

int main() {
    std::cout << "Starting CUDA Issue 7 Dummy Risk Test..." << std::endl;

    std::size_t count = 1000;
    auto orders = titanium::generate_dummy_orders(count);
    
    std::vector<float> results(count, 0.0f);

    std::cout << "Allocating and launching GPU kernel for " << count << " orders." << std::endl;

    try {
        titanium::run_dummy_risk_check(orders.data(), count, results.data());
    } catch (const std::exception& e) {
        std::cerr << "GPU Error: " << e.what() << std::endl;
        return 1;
    }

    // Verify
    bool success = true;
    for (std::size_t i = 0; i < count; ++i) {
        float p = static_cast<float>(orders[i].price);
        float q = static_cast<float>(orders[i].quantity);
        float v = 0.05f;
        
        for (int j = 0; j < 50; ++j) {
            p = p * std::cos(v) + std::sin(p) * 0.01f;
            q = q * std::exp(-v) + 1.0f;
            v += 0.001f;
        }
        float expected = p * q * v;
        
        // Use a slightly larger epsilon for complex float math comparisons
        if (std::abs(results[i] - expected) > 1e-2) {
            std::cerr << "Verification failed at index " << i << "! Expected " << expected << " but got " << results[i] << std::endl;
            success = false;
            break;
        }
    }

    if (success) {
        std::cout << "Verification SUCCESS! The GPU calculated the risk variables correctly." << std::endl;
    } else {
        std::cout << "Verification FAILED." << std::endl;
        return 1;
    }

    return 0;
}
