#pragma once

#include <cstddef>
#include "titanium/order.hpp"

namespace titanium {

/**
 * @brief CPU implementation of the risk check.
 * 
 * @param orders Array of orders to process.
 * @param count Number of orders in the array.
 * @param results Output array where results will be stored.
 */
inline void run_risk_check_cpu(const Order* orders, std::size_t count, float* results) {
    for (std::size_t i = 0; i < count; ++i) {
        float p = static_cast<float>(orders[i].price);
        float q = static_cast<float>(orders[i].quantity);
        float v = 0.05f;
        
        for (int j = 0; j < 50; ++j) {
            p = p * std::cos(v) + std::sin(p) * 0.01f;
            q = q * std::exp(-v) + 1.0f;
            v += 0.001f;
        }

        results[i] = p * q * v;
    }
}

} // namespace titanium
