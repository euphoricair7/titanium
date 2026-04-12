#include "titanium/engine/baseline_engine.hpp"

namespace titanium {

void BaselineEngine::process_order(Order order) {
    if (order.side == Side::Buy) {
        // Match against lowest ask prices
        while (order.quantity > 0 && !asks_.empty()) {
            // Find best (lowest) ask price
            auto best_it = std::min_element(
                asks_.begin(), asks_.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            uint32_t best_price = best_it->first;
            if (best_price > order.price) break;  // limit price not met

            auto& queue = best_it->second;
            while (!queue.empty() && order.quantity > 0) {
                Order& resting = queue.front();
                if (resting.quantity <= order.quantity) {
                    order.quantity -= resting.quantity;
                    queue.pop_front();  // O(1)
                } else {
                    resting.quantity -= order.quantity;
                    order.quantity = 0;
                }
            }
            if (queue.empty()) {
                asks_.erase(best_it);
            }
        }
        // Add leftover to bids
        if (order.quantity > 0) {
            bids_[order.price].push_back(order);
        }
    } else {
        // Match against highest bid prices
        while (order.quantity > 0 && !bids_.empty()) {
            // Find best (highest) bid price
            auto best_it = std::max_element(
                bids_.begin(), bids_.end(),
                [](const auto& a, const auto& b) { return a.first < b.first; });

            uint32_t best_price = best_it->first;
            if (best_price < order.price) break;  // limit price not met

            auto& queue = best_it->second;
            while (!queue.empty() && order.quantity > 0) {
                Order& resting = queue.front();
                if (resting.quantity <= order.quantity) {
                    order.quantity -= resting.quantity;
                    queue.pop_front();  // O(1)
                } else {
                    resting.quantity -= order.quantity;
                    order.quantity = 0;
                }
            }
            if (queue.empty()) {
                bids_.erase(best_it);
            }
        }
        // Add leftover to asks
        if (order.quantity > 0) {
            asks_[order.price].push_back(order);
        }
    }
}

} // namespace titanium