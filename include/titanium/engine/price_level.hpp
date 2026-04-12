#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include "titanium/order.hpp"

namespace titanium {

/**
 * @brief Represents the data for a single price level, using a ring buffer.
 */
struct PriceLevelData {
    static constexpr std::size_t INLINE_CAPACITY = 16;

    uint32_t total_quantity = 0;
    uint32_t order_count = 0;
    uint32_t head_ = 0;
    Order inline_orders_[INLINE_CAPACITY];
    std::vector<Order> dynamic_orders_;

    // ----- Element access -----
    Order& front() {
        if (head_ < INLINE_CAPACITY) {
            return inline_orders_[head_];
        }
        return dynamic_orders_[head_ - INLINE_CAPACITY];
    }

    const Order& front() const {
        if (head_ < INLINE_CAPACITY) {
            return inline_orders_[head_];
        }
        return dynamic_orders_[head_ - INLINE_CAPACITY];
    }

    Order& at(std::size_t logical_index) {
        const std::size_t physical_index = head_ + logical_index;
        if (physical_index < INLINE_CAPACITY) {
            return inline_orders_[physical_index];
        }
        return dynamic_orders_[physical_index - INLINE_CAPACITY];
    }

    const Order& at(std::size_t logical_index) const {
        const std::size_t physical_index = head_ + logical_index;
        if (physical_index < INLINE_CAPACITY) {
            return inline_orders_[physical_index];
        }
        return dynamic_orders_[physical_index - INLINE_CAPACITY];
    }

    // ----- Modifiers -----
    void add_order(const Order& order) {
        const std::size_t physical_index = head_ + order_count;
        if (physical_index < INLINE_CAPACITY) {
            inline_orders_[physical_index] = order;
        } else {
            if (dynamic_orders_.empty()) {
                dynamic_orders_.reserve(INLINE_CAPACITY);
            }
            dynamic_orders_.push_back(order);
        }
        ++order_count;
        total_quantity += order.quantity;
    }

    void pop_front() {
        if (order_count == 0) return;
        total_quantity -= front().quantity;
        ++head_;
        --order_count;

        if (order_count == 0) {
            head_ = 0;
            if (!dynamic_orders_.empty()) {
                dynamic_orders_.clear();
            }
        }
    }

    void erase_at(std::size_t logical_index) {
        if (logical_index >= order_count) return;
        total_quantity -= at(logical_index).quantity;

        const std::size_t physical_index = head_ + logical_index;
        if (physical_index < INLINE_CAPACITY) {
            // Shift inline elements
            for (std::size_t i = physical_index; i < head_ + order_count - 1; ++i) {
                if (i + 1 < INLINE_CAPACITY) {
                    inline_orders_[i] = inline_orders_[i + 1];
                } else {
                    inline_orders_[i] = dynamic_orders_.front();
                    dynamic_orders_.erase(dynamic_orders_.begin());
                }
            }
        } else {
            // Erase from vector
            dynamic_orders_.erase(dynamic_orders_.begin() + (physical_index - INLINE_CAPACITY));
        }
        --order_count;
    }

    void reset() {
        total_quantity = 0;
        order_count = 0;
        head_ = 0;
        if (!dynamic_orders_.empty()) {
            dynamic_orders_.clear();
        }
    }
};

} // namespace titanium