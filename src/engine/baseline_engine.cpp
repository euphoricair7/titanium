#include "titanium/engine/baseline_engine.hpp"

namespace titanium {

void BaselineEngine::process_order(Order order) {
    // Wrap the entire function in a lock to simulate the baseline bottleneck
    std::lock_guard<Spinlock> lock(engine_mutex_);

    if (order.side == Side::Buy) {
        // Match Buy order against the lowest Ask prices
        auto it = asks_.begin();
        while (it != asks_.end() && order.quantity > 0 && it->first <= order.price) {
            auto& orders_at_price = it->second;
            auto order_it = orders_at_price.begin();
            
            while (order_it != orders_at_price.end() && order.quantity > 0) {
                if (order_it->quantity <= order.quantity) {
                    order.quantity -= order_it->quantity;
                    order_it = orders_at_price.erase(order_it); // Fully filled
                } else {
                    order_it->quantity -= order.quantity; // Partially filled
                    order.quantity = 0;
                    break;
                }
            }
            if (orders_at_price.empty()) { 
                it = asks_.erase(it); 
            } else { 
                ++it; 
            }
        }
        // If there is leftover quantity, add it to the Bids book
        if (order.quantity > 0) { 
            bids_[order.price].push_back(order); 
        }
    } else {
        // Match Sell order against the highest Bid prices
        auto it = bids_.begin();
        while (it != bids_.end() && order.quantity > 0 && it->first >= order.price) {
            auto& orders_at_price = it->second;
            auto order_it = orders_at_price.begin();
            
            while (order_it != orders_at_price.end() && order.quantity > 0) {
                if (order_it->quantity <= order.quantity) {
                    order.quantity -= order_it->quantity;
                    order_it = orders_at_price.erase(order_it);
                } else {
                    order_it->quantity -= order.quantity;
                    order.quantity = 0;
                    break;
                }
            }
            if (orders_at_price.empty()) { 
                it = bids_.erase(it); 
            } else { 
                ++it; 
            }
        }
        // If there is leftover quantity, add it to the Asks book
        if (order.quantity > 0) { 
            asks_[order.price].push_back(order); 
        }
    }
}

} // namespace titanium
