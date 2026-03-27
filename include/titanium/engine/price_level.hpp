#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include "titanium/order.hpp"

namespace titanium {

/**
 * @brief Represents the data for a single price level, excluding the price itself.
 * 
 * Separating PriceLevelData from the price allows for SoA (Structure of Arrays)
 * layout, enabling SIMD price scanning.
 */
struct PriceLevelData {
    uint32_t total_quantity = 0;
    static constexpr std::size_t MAX_ORDERS_PER_LEVEL = 8;
    std::array<Order, MAX_ORDERS_PER_LEVEL> orders;
    std::uint32_t order_count = 0;

    void add_order(const Order& order) {
        if (order_count < MAX_ORDERS_PER_LEVEL) {
            orders[order_count++] = order;
            total_quantity += order.quantity;
        }
    }
};

} // namespace titanium
