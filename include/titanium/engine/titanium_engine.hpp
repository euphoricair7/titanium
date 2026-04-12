#pragma once

#include <vector>
#include <memory>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include "titanium/engine/price_level.hpp"

namespace titanium {
struct OrderLocation {
    Side side;
    uint32_t price;
};

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

        // Batched / Async Stats
        std::uint64_t batched_calls = 0;
        std::uint64_t batched_total_ns = 0;
        std::uint64_t batched_orders = 0;
        std::uint64_t batched_memcpy_ns = 0;
        std::uint64_t batched_sync_ns = 0;
        std::uint64_t batched_submit_ns = 0;
        std::uint64_t batched_cpu_process_ns = 0;
    };
    struct DebugCounters {
    std::uint64_t outer_loop_count = 0;
    std::uint64_t inner_loop_count = 0;
    } debug_;

    static constexpr std::size_t MAX_LEVELS = 104;
    static constexpr std::size_t PRICE_WINDOW_SIZE = 262144;
    static constexpr std::size_t ORDER_TRACKER_CAP = 1'000'000;

    TitaniumEngine();
    ~TitaniumEngine();
    TitaniumEngine(const TitaniumEngine&) = delete;
    TitaniumEngine& operator=(const TitaniumEngine&) = delete;
    TitaniumEngine(TitaniumEngine&&) = delete;
    TitaniumEngine& operator=(TitaniumEngine&&) = delete;

    void process_order(Order order);
    void process_orders_batched(const Order* orders, std::size_t total_count);
    bool cancel_order(uint64_t order_id);

    std::size_t get_bid_count() const { return bid_count_; }
    std::size_t get_ask_count() const { return ask_count_; }
    std::size_t get_tracker_size() const { return tracker_size_; }
    const ProfileStats& profile_stats() const { return profile_stats_; }
    void reset_profile_stats() { profile_stats_ = {}; }

private:
    struct OrderMetadata {
        uint32_t price = 0;
        uint8_t side = static_cast<uint8_t>(Side::Buy);
        uint8_t active = 0;
        uint16_t reserved = 0;
    };
    static_assert(sizeof(OrderMetadata) == 8, "OrderMetadata should stay cache-dense (8 bytes)");

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
    bool lookup_order(uint64_t order_id, OrderLocation& out) const;

    std::unique_ptr<OrderMetadata[]> tracker_pool_;
    std::size_t tracker_size_ = 0;

    uint32_t bid_base_price_ = 0;
    uint32_t ask_base_price_ = 0;
    bool bid_base_initialized_ = false;
    bool ask_base_initialized_ = false;
    int best_bid_index_ = -1;
    int best_ask_index_ = -1;
    PriceLevelData* cached_best_bid_ = nullptr;
    PriceLevelData* cached_best_ask_ = nullptr;

    std::vector<PriceLevelData> bid_data_;
    std::vector<PriceLevelData> ask_data_;
    std::vector<uint8_t> bid_active_;
    std::vector<uint8_t> ask_active_;
    std::bitset<PRICE_WINDOW_SIZE> bid_active_levels_;
    std::bitset<PRICE_WINDOW_SIZE> ask_active_levels_;

    std::size_t bid_window_level_count_ = 0;
    std::size_t ask_window_level_count_ = 0;
    std::size_t bid_count_ = 0;
    std::size_t ask_count_ = 0;

    ProfileStats profile_stats_;
};

} // namespace titanium

