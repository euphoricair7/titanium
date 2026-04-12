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
        const Order& o = orders[i];
        // Identical "dummy" risk logic as the GPU kernel
        if (o.price > 1000000 || o.quantity > 500000) {
            results[i] = -1.0f;
        } else {
            results[i] = 1.0f;
        }
    }
}

} // namespace titanium
