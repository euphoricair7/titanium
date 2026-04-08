#pragma once

#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>
#include "titanium/order.hpp"
#include "titanium/engine/price_level.hpp"

namespace titanium {
class AsyncRiskEngine;

struct OrderLocation {
    Side side;
    uint32_t price;
};

/**
 * @brief An ultra-optimized matching engine using AVX2 SIMD for price scanning.
 * 
 * This engine uses a Structure-of-Arrays (SoA) layout to allow 8 price 
 * comparisons per cycle using AVX2 instructions.
 */
class TitaniumEngine {
public:
    struct ProfileStats {
        std::uint64_t process_order_calls = 0;
        std::uint64_t process_order_ns = 0;
        std::uint64_t match_buy_calls = 0;
        std::uint64_t match_buy_ns = 0;
        std::uint64_t match_sell_calls = 0;
        std::uint64_t match_sell_ns = 0;
        std::uint64_t add_bid_calls = 0;
        std::uint64_t add_bid_ns = 0;
        std::uint64_t add_bid_search_ns = 0;
        std::uint64_t add_bid_shift_ns = 0;
        std::uint64_t add_bid_tracker_ns = 0;
        std::uint64_t add_ask_calls = 0;
        std::uint64_t add_ask_ns = 0;
        std::uint64_t add_ask_search_ns = 0;
        std::uint64_t add_ask_shift_ns = 0;
        std::uint64_t add_ask_tracker_ns = 0;
        std::uint64_t batched_calls = 0;
        std::uint64_t batched_total_ns = 0;
        std::uint64_t batched_cpu_process_ns = 0;
        std::uint64_t batched_memcpy_ns = 0;
        std::uint64_t batched_sync_ns = 0;
        std::uint64_t batched_submit_ns = 0;
        std::uint64_t batched_orders = 0;
    };

    static constexpr std::size_t MAX_LEVELS = 104;
    static constexpr std::size_t PRICE_WINDOW_SIZE = 8192;
    static constexpr std::size_t ORDER_TRACKER_CAP = 2'000'000;
    static constexpr std::size_t BATCH_SIZE = 1024;

    TitaniumEngine();
    ~TitaniumEngine();
    TitaniumEngine(const TitaniumEngine&) = delete;
    TitaniumEngine& operator=(const TitaniumEngine&) = delete;
    TitaniumEngine(TitaniumEngine&&) = delete;
    TitaniumEngine& operator=(TitaniumEngine&&) = delete;

    void process_order(Order order);
    void process_orders_batched(const Order* orders, std::size_t total_count);
    bool cancel_order(uint64_t order_id);

    // For verification/benchmarking
    std::size_t get_bid_count() const { return bid_count_; }
    std::size_t get_ask_count() const { return ask_count_; }
    std::size_t get_tracker_size() const { return tracker_size_; }
    const ProfileStats& profile_stats() const { return profile_stats_; }
    void reset_profile_stats() { profile_stats_ = {}; }

private:
    struct FlatOrderMeta {
        uint32_t price = 0;
        Side side = Side::Buy;
        bool active = false;
    };

    void match_buy(Order& order);
    void match_sell(Order& order);
    void add_to_bids(const Order& order);
    void add_to_asks(const Order& order);
    bool ensure_bid_window(uint32_t price);
    bool ensure_ask_window(uint32_t price);
    bool price_in_bid_window(uint32_t price) const;
    bool price_in_ask_window(uint32_t price) const;
    std::size_t bid_index(uint32_t price) const;
    std::size_t ask_index(uint32_t price) const;
    void clear_bid_level(std::size_t idx);
    void clear_ask_level(std::size_t idx);
    PriceLevelData* best_bid_level(uint32_t& price);
    PriceLevelData* best_ask_level(uint32_t& price);
    void update_best_bid_after_removal(std::size_t removed_idx);
    void update_best_ask_after_removal(std::size_t removed_idx);
    PriceLevelData* find_level(Side side, uint32_t price);
    void untrack_order(uint64_t order_id);
    void track_order(uint64_t order_id, Side side, uint32_t price);
    bool lookup_order(uint64_t order_id, OrderLocation& out) const;
    void ensure_gpu_pipeline();

    std::unordered_map<uint64_t, OrderLocation> overflow_order_tracker_;
    std::vector<FlatOrderMeta> order_metadata_;
    std::size_t tracker_size_ = 0;

    uint32_t bid_base_price_ = 0;
    uint32_t ask_base_price_ = 0;
    bool bid_base_initialized_ = false;
    bool ask_base_initialized_ = false;
    int best_bid_index_ = -1;
    int best_ask_index_ = -1;

    std::vector<PriceLevelData> bid_data_;
    std::vector<PriceLevelData> ask_data_;
    std::vector<uint8_t> bid_active_;
    std::vector<uint8_t> ask_active_;
    std::bitset<PRICE_WINDOW_SIZE> bid_active_levels_;
    std::bitset<PRICE_WINDOW_SIZE> ask_active_levels_;
    std::map<uint32_t, PriceLevelData, std::greater<uint32_t>> overflow_bids_;
    std::map<uint32_t, PriceLevelData, std::less<uint32_t>> overflow_asks_;

    std::size_t bid_window_level_count_ = 0;
    std::size_t ask_window_level_count_ = 0;
    std::size_t bid_count_ = 0;
    std::size_t ask_count_ = 0;

    std::unique_ptr<AsyncRiskEngine> async_risk_engine_;
    Order* pinned_orders_[2] = {nullptr, nullptr};
    float* pinned_results_[2] = {nullptr, nullptr};

    ProfileStats profile_stats_;
};

} // namespace titanium
