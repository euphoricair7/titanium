#pragma once

#include <array>
#include <cstddef>
#include "titanium/order.hpp"
#include "titanium/engine/price_level.hpp"

namespace titanium {

/**
 * @brief An optimized matching engine using contiguous arrays for price levels.
 * 
 * This engine replaces tree-based maps with flat arrays to improve CPU cache locality.
 * Matching is performed via linear scans of the best bids and asks.
 */
class TitaniumEngine {
public:
    static constexpr std::size_t MAX_LEVELS = 100;

    void process_order(Order order);

    // For verification/benchmarking
    std::size_t get_bid_count() const { return bid_count_; }
    std::size_t get_ask_count() const { return ask_count_; }

private:
    void match_buy(Order& order);
    void match_sell(Order& order);
    void add_to_bids(const Order& order);
    void add_to_asks(const Order& order);

    alignas(64) std::array<PriceLevel, MAX_LEVELS> bids_;
    alignas(64) std::array<PriceLevel, MAX_LEVELS> asks_;
    
    std::size_t bid_count_ = 0;
    std::size_t ask_count_ = 0;
};

} // namespace titanium
