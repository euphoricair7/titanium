#pragma once

#include <array>
#include <cstddef>
#include <unordered_map>
#include <vector>
#include <immintrin.h> // SIMD AVX2
#include "titanium/order.hpp"
#include "titanium/engine/price_level.hpp"
#include "titanium/concurrency/spsc_queue.hpp"

namespace titanium {

struct OrderLocation {
    Side side;
    uint32_t price;
};

struct TradeReceipt {
    uint64_t buyer_id;
    uint64_t seller_id;
    uint32_t price;
    uint32_t quantity;
};

/**
 * @brief An ultra-optimized matching engine using AVX2 SIMD for price scanning.
 * 
 * This engine uses a Structure-of-Arrays (SoA) layout to allow 8 price 
 * comparisons per cycle using AVX2 instructions.
 */
class TitaniumEngine {
public:
    // Padded to multiple of 8 for SIMD
    static constexpr std::size_t MAX_LEVELS = 104; 

    void process_order(Order order);
    void process_order_heavy_cpu(Order order);
    void process_orders_batched(const Order* orders, std::size_t total_count);
    bool cancel_order(uint64_t order_id);

    // For verification/benchmarking
    std::size_t get_bid_count() const { return bid_count_; }
    std::size_t get_ask_count() const { return ask_count_; }

private:
    std::vector<TradeReceipt> pending_trades_;
    SPSCQueue<uint64_t, 65536> cancel_queue_;

    void process_gpu_cancellations();
    void shred_receipts(uint64_t order_id);

    void match_buy(Order& order);
    void match_sell(Order& order);
    void add_to_bids(const Order& order);
    void add_to_asks(const Order& order);

    std::unordered_map<uint64_t, OrderLocation> order_tracker_;

    // SoA layout for SIMD
    alignas(32) std::array<uint32_t, MAX_LEVELS> bid_prices_;
    alignas(32) std::array<uint32_t, MAX_LEVELS> ask_prices_;
    
    std::array<PriceLevelData, MAX_LEVELS> bid_data_;
    std::array<PriceLevelData, MAX_LEVELS> ask_data_;
    
    std::size_t bid_count_ = 0;
    std::size_t ask_count_ = 0;
};

} // namespace titanium
