#include "titanium/engine/titanium_engine.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#ifdef TITANIUM_ENABLE_PROFILING
#undef TITANIUM_ENABLE_PROFILING
#endif
#define TITANIUM_ENABLE_PROFILING 0
#if defined(_MSC_VER)
#include <xmmintrin.h>
#endif

#if defined(_MSC_VER) && defined(_WIN32)
#include <intrin.h>
#define NOMINMAX
#include <windows.h>
#endif

#include "titanium/engine/cuda/risk_kernel.cuh"

namespace titanium {
namespace {
#ifndef TITANIUM_FINE_GRAIN_PROFILE
#define TITANIUM_FINE_GRAIN_PROFILE 0
#endif

#ifndef TITANIUM_ENABLE_PROFILING
#define TITANIUM_ENABLE_PROFILING 0
#endif

#ifndef TITANIUM_GPU_THROUGHPUT_MODE
#define TITANIUM_GPU_THROUGHPUT_MODE 1
#endif

#if TITANIUM_FIXED_WINDOW
#pragma message("TITANIUM_FIXED_WINDOW is enabled")
#else
#pragma message("TITANIUM_FIXED_WINDOW is DISABLED")
#endif

inline void reset_level_metadata(PriceLevelData& level) {
    level.reset();
}

inline std::uint64_t now_ns() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
}

constexpr std::size_t kWordBits = 64;
constexpr std::size_t kActiveWordCount =
    TitaniumEngine::PRICE_WINDOW_SIZE / kWordBits;
static_assert(TitaniumEngine::PRICE_WINDOW_SIZE % kWordBits == 0,
    "PRICE_WINDOW_SIZE must be divisible by 64");

int find_highest_active_index(const std::bitset<TitaniumEngine::PRICE_WINDOW_SIZE>& bits, std::size_t start_idx = TitaniumEngine::PRICE_WINDOW_SIZE - 1) {
#if defined(_MSC_VER) && defined(_WIN32)
    const auto* words = reinterpret_cast<const unsigned __int64*>(&bits);
    for (std::size_t word = (start_idx / kWordBits) + 1; word-- > 0;) {
        if (words[word] == 0) continue;
        unsigned long bit = 0;
        if (_BitScanReverse64(&bit, words[word])) {
            return static_cast<int>(word * kWordBits + bit);
        }
    }
    return -1;
#else
    for (int i = static_cast<int>(start_idx); i >= 0; --i) {
        if (bits.test(static_cast<std::size_t>(i))) {
            return i;
        }
    }
    return -1;
#endif
}

int find_lowest_active_index(const std::bitset<TitaniumEngine::PRICE_WINDOW_SIZE>& bits, std::size_t start_idx = 0) {
#if defined(_MSC_VER) && defined(_WIN32)
    const auto* words = reinterpret_cast<const unsigned __int64*>(&bits);
    for (std::size_t word = start_idx / kWordBits; word < kActiveWordCount; ++word) {
        if (words[word] == 0) continue;
        unsigned long bit = 0;
        if (_BitScanForward64(&bit, words[word])) {
            return static_cast<int>(word * kWordBits + bit);
        }
    }
    return -1;
#else
    for (std::size_t i = start_idx; i < TitaniumEngine::PRICE_WINDOW_SIZE; ++i) {
        if (bits.test(i)) {
            return static_cast<int>(i);
        }
    }
    return -1;
#endif
}
} // namespace

TitaniumEngine::TitaniumEngine() {
    tracker_pool_ = std::make_unique<OrderMetadata[]>(ORDER_TRACKER_CAP);
    bid_data_.resize(PRICE_WINDOW_SIZE);
    ask_data_.resize(PRICE_WINDOW_SIZE);
    bid_active_.resize(PRICE_WINDOW_SIZE, 0);
    ask_active_.resize(PRICE_WINDOW_SIZE, 0);
}

TitaniumEngine::~TitaniumEngine() {
}

// GPU and Commit thread methods removed for performance rollback

bool TitaniumEngine::price_in_bid_window(uint32_t price) const {
    if (!bid_base_initialized_) return false;
    return price >= bid_base_price_ &&
           static_cast<std::size_t>(price - bid_base_price_) < PRICE_WINDOW_SIZE;
}

bool TitaniumEngine::price_in_ask_window(uint32_t price) const {
    if (!ask_base_initialized_) return false;
    return price >= ask_base_price_ &&
           static_cast<std::size_t>(price - ask_base_price_) < PRICE_WINDOW_SIZE;
}

std::size_t TitaniumEngine::bid_index(uint32_t price) const {
    return static_cast<std::size_t>(price - bid_base_price_);
}

std::size_t TitaniumEngine::ask_index(uint32_t price) const {
    return static_cast<std::size_t>(price - ask_base_price_);
}

bool TitaniumEngine::ensure_bid_window(uint32_t price) {
#if TITANIUM_FIXED_WINDOW
    if (!bid_base_initialized_) {
        bid_base_price_ = 0;
        bid_base_initialized_ = true;
    }
    return true;
#else
    const uint32_t half = static_cast<uint32_t>(PRICE_WINDOW_SIZE / 2);
    if (!bid_base_initialized_) {
        bid_base_price_ = (price > half) ? (price - half) : 0;
        bid_base_initialized_ = true;
        return true;
    }
    if (price_in_bid_window(price)) return true;
    if (bid_window_level_count_ == 0) {
        bid_base_price_ = (price > half) ? (price - half) : 0;
        best_bid_index_ = -1;
        cached_best_bid_ = nullptr;
        return true;
    }
    return false;
#endif
}

bool TitaniumEngine::ensure_ask_window(uint32_t price) {
#if TITANIUM_FIXED_WINDOW
    if (!ask_base_initialized_) {
        ask_base_price_ = 0;
        ask_base_initialized_ = true;
    }
    return true;
#else
    const uint32_t half = static_cast<uint32_t>(PRICE_WINDOW_SIZE / 2);
    if (!ask_base_initialized_) {
        ask_base_price_ = (price > half) ? (price - half) : 0;
        ask_base_initialized_ = true;
        return true;
    }
    if (price_in_ask_window(price)) return true;
    if (ask_window_level_count_ == 0) {
        ask_base_price_ = (price > half) ? (price - half) : 0;
        best_ask_index_ = -1;
        cached_best_ask_ = nullptr;
        return true;
    }
    return false;
#endif
}

void TitaniumEngine::update_best_bid_after_removal(std::size_t removed_idx) {
    if (best_bid_index_ != static_cast<int>(removed_idx)) return;
    best_bid_index_ = find_highest_active_index(bid_active_levels_, removed_idx);
    cached_best_bid_ = (best_bid_index_ >= 0)
        ? &bid_data_[static_cast<std::size_t>(best_bid_index_)]
        : nullptr;
}

void TitaniumEngine::update_best_ask_after_removal(std::size_t removed_idx) {
    if (best_ask_index_ != static_cast<int>(removed_idx)) return;
    best_ask_index_ = find_lowest_active_index(ask_active_levels_, removed_idx);
    cached_best_ask_ = (best_ask_index_ >= 0)
        ? &ask_data_[static_cast<std::size_t>(best_ask_index_)]
        : nullptr;
}

void TitaniumEngine::clear_bid_level(std::size_t idx) {
    if (!bid_active_[idx]) return;
    bid_active_[idx] = 0;
    bid_active_levels_.reset(idx);
    reset_level_metadata(bid_data_[idx]);
    bid_window_level_count_--;
    bid_count_--;
    update_best_bid_after_removal(idx);
}

void TitaniumEngine::clear_ask_level(std::size_t idx) {
    if (!ask_active_[idx]) return;
    ask_active_[idx] = 0;
    ask_active_levels_.reset(idx);
    reset_level_metadata(ask_data_[idx]);
    ask_window_level_count_--;
    ask_count_--;
    update_best_ask_after_removal(idx);
}

PriceLevelData* TitaniumEngine::best_bid_level(uint32_t& price) {
    if (cached_best_bid_) {
        price = bid_base_price_ + static_cast<uint32_t>(best_bid_index_);
        return cached_best_bid_;
    }
    return nullptr;
}

PriceLevelData* TitaniumEngine::best_ask_level(uint32_t& price) {
    if (cached_best_ask_) {
        price = ask_base_price_ + static_cast<uint32_t>(best_ask_index_);
        return cached_best_ask_;
    }
    return nullptr;
}

void TitaniumEngine::untrack_order(uint64_t order_id) {
    if (order_id >= ORDER_TRACKER_CAP) return;

    auto& meta = tracker_pool_[static_cast<std::size_t>(order_id)];
    if (meta.active) {
        meta.active = false;
        tracker_size_--;
    }
}

bool TitaniumEngine::lookup_order(uint64_t order_id, OrderLocation& out) const {
    if (order_id >= ORDER_TRACKER_CAP) return false;

    const auto& meta = tracker_pool_[static_cast<std::size_t>(order_id)];
    if (!meta.active) return false;
    out = OrderLocation{static_cast<Side>(meta.side), meta.price};
    return true;
}

PriceLevelData* TitaniumEngine::find_level(Side side, uint32_t price) {
    if (side == Side::Buy) {
        if (price_in_bid_window(price)) {
            std::size_t idx = bid_index(price);
            if (bid_active_[idx]) return &bid_data_[idx];
        }
        return nullptr;
    }

    if (price_in_ask_window(price)) {
        std::size_t idx = ask_index(price);
        if (ask_active_[idx]) return &ask_data_[idx];
    }
    return nullptr;
}

void TitaniumEngine::process_order(Order order) {
#if TITANIUM_ENABLE_PROFILING
    const auto start_ns = now_ns();
#endif
    if (order.side == Side::Buy) {
        match_buy(order);
        if (order.quantity > 0 && order.type == OrderType::Limit) {
            add_to_bids(order);
        }
    } else {
        match_sell(order);
        if (order.quantity > 0 && order.type == OrderType::Limit) {
            add_to_asks(order);
        }
    }
#if TITANIUM_ENABLE_PROFILING
    profile_stats_.process_order_calls++;
    profile_stats_.process_order_ns += (now_ns() - start_ns);
#endif
}

void TitaniumEngine::process_orders_batched(const Order* orders,
                                            std::size_t total_count) {
#if TITANIUM_ENABLE_PROFILING
    const auto start_ns = now_ns();
#endif
    for (std::size_t i = 0; i < total_count; ++i) {
        process_order(orders[i]);
    }
#if TITANIUM_ENABLE_PROFILING
    profile_stats_.batched_calls++;
    profile_stats_.batched_orders += total_count;
    profile_stats_.batched_total_ns += (now_ns() - start_ns);
    // Note: Other batched stats (memcpy, sync, etc.) are 0 in this CPU-only baseline loop.
#endif
}

bool TitaniumEngine::cancel_order(uint64_t order_id) {
    OrderLocation loc{};
    if (!lookup_order(order_id, loc)) return false;

    PriceLevelData* level = find_level(loc.side, loc.price);
    if (!level) return false;

    for (std::uint32_t j = 0; j < level->order_count; ++j) {
        if (level->at(j).id != order_id) continue;

        level->total_quantity -= level->at(j).quantity;
        level->erase_at(j);
        untrack_order(order_id);

        if (level->order_count == 0) {
            if (loc.side == Side::Buy) {
                if (price_in_bid_window(loc.price)) {
                    clear_bid_level(bid_index(loc.price));
                }
            } else {
                if (price_in_ask_window(loc.price)) {
                    clear_ask_level(ask_index(loc.price));
                }
            }
        }
        return true;
    }
    return false;
}

void TitaniumEngine::match_buy(Order& order) {
#if TITANIUM_ENABLE_PROFILING
    const auto start_ns = now_ns();
#endif
    while (order.quantity > 0) {
        uint32_t best_price = 0;
        PriceLevelData* level = best_ask_level(best_price);

        if (!level) break;
        if (order.type == OrderType::Limit && best_price > order.price) break;

        while (level->order_count > 0 && order.quantity > 0) {
            Order& resting = level->front();
            const uint32_t traded = std::min(resting.quantity, order.quantity);
            order.quantity -= traded;
            resting.quantity -= traded;
            level->total_quantity -= traded;

            if (resting.quantity == 0) {
                untrack_order(resting.id);
                level->pop_front();
            } else {
                break;
            }
        }

        if (level->order_count == 0) {
            if (price_in_ask_window(best_price)) {
                clear_ask_level(ask_index(best_price));
            }
        }
    }
#if TITANIUM_ENABLE_PROFILING
    profile_stats_.match_buy_calls++;
    profile_stats_.match_buy_ns += (now_ns() - start_ns);
#endif
}

void TitaniumEngine::match_sell(Order& order) {
#if TITANIUM_ENABLE_PROFILING
    const auto start_ns = now_ns();
#endif
    while (order.quantity > 0) {
        uint32_t best_price = 0;
        PriceLevelData* level = best_bid_level(best_price);

        if (!level) break;
        if (order.type == OrderType::Limit && best_price < order.price) break;

        while (level->order_count > 0 && order.quantity > 0) {
            Order& resting = level->front();
            const uint32_t traded = std::min(resting.quantity, order.quantity);
            order.quantity -= traded;
            resting.quantity -= traded;
            level->total_quantity -= traded;

            if (resting.quantity == 0) {
                untrack_order(resting.id);
                level->pop_front();
            } else {
                break;
            }
        }

        if (level->order_count == 0) {
            if (price_in_bid_window(best_price)) {
                clear_bid_level(bid_index(best_price));
            }
        }
    }
#if TITANIUM_ENABLE_PROFILING
    profile_stats_.match_sell_calls++;
    profile_stats_.match_sell_ns += (now_ns() - start_ns);
#endif
}

void TitaniumEngine::add_to_bids(const Order& order) {
#if TITANIUM_ENABLE_PROFILING
    const auto start_ns = now_ns();
#endif
#if TITANIUM_FINE_GRAIN_PROFILE
    const auto search_start = now_ns();
#endif
    bool use_window = ensure_bid_window(order.price) && price_in_bid_window(order.price);
#if TITANIUM_ENABLE_PROFILING && TITANIUM_FINE_GRAIN_PROFILE
    profile_stats_.add_bid_search_ns += (now_ns() - search_start);
#endif

#if TITANIUM_FINE_GRAIN_PROFILE
    auto tracker_start = now_ns();
#endif
    if (order.id < ORDER_TRACKER_CAP) {
#if defined(_MSC_VER)
        _mm_prefetch(reinterpret_cast<const char*>(&tracker_pool_[static_cast<std::size_t>(order.id)]), _MM_HINT_T0);
#endif
        auto& meta = tracker_pool_[static_cast<std::size_t>(order.id)];
        if (!meta.active) tracker_size_++;
        meta.active = 1;
        meta.side = static_cast<uint8_t>(Side::Buy);
        meta.price = order.price;
    }
#if TITANIUM_ENABLE_PROFILING && TITANIUM_FINE_GRAIN_PROFILE
    profile_stats_.add_bid_tracker_ns += (now_ns() - tracker_start);
#endif

    if (use_window) {
        std::size_t idx = bid_index(order.price);
        if (!bid_active_[idx]) {
            bid_active_[idx] = 1;
            bid_active_levels_.set(idx);
            bid_data_[idx].reset();
            bid_window_level_count_++;
            bid_count_++;
            if (best_bid_index_ < 0 || static_cast<int>(idx) > best_bid_index_) {
                best_bid_index_ = static_cast<int>(idx);
                cached_best_bid_ = &bid_data_[idx];
            }
            if (!cached_best_bid_) cached_best_bid_ = &bid_data_[idx];
        }
        bid_data_[idx].add_order(order);
    }

#if TITANIUM_ENABLE_PROFILING
    profile_stats_.add_bid_calls++;
    profile_stats_.add_bid_ns += (now_ns() - start_ns);
#endif
}

void TitaniumEngine::add_to_asks(const Order& order) {
#if TITANIUM_ENABLE_PROFILING
    const auto start_ns = now_ns();
#endif
#if TITANIUM_FINE_GRAIN_PROFILE
    const auto search_start = now_ns();
#endif
    bool use_window = ensure_ask_window(order.price) && price_in_ask_window(order.price);
#if TITANIUM_ENABLE_PROFILING && TITANIUM_FINE_GRAIN_PROFILE
    profile_stats_.add_ask_search_ns += (now_ns() - search_start);
#endif

#if TITANIUM_FINE_GRAIN_PROFILE
    auto tracker_start = now_ns();
#endif
    if (order.id < ORDER_TRACKER_CAP) {
#if defined(_MSC_VER)
        _mm_prefetch(reinterpret_cast<const char*>(&tracker_pool_[static_cast<std::size_t>(order.id)]), _MM_HINT_T0);
#endif
        auto& meta = tracker_pool_[static_cast<std::size_t>(order.id)];
        if (!meta.active) tracker_size_++;
        meta.active = 1;
        meta.side = static_cast<uint8_t>(Side::Sell);
        meta.price = order.price;
    }
#if TITANIUM_ENABLE_PROFILING && TITANIUM_FINE_GRAIN_PROFILE
    profile_stats_.add_ask_tracker_ns += (now_ns() - tracker_start);
#endif

    if (use_window) {
        std::size_t idx = ask_index(order.price);
        if (!ask_active_[idx]) {
            ask_active_[idx] = 1;
            ask_active_levels_.set(idx);
            ask_data_[idx].reset();
            ask_window_level_count_++;
            ask_count_++;
            if (best_ask_index_ < 0 || static_cast<int>(idx) < best_ask_index_) {
                best_ask_index_ = static_cast<int>(idx);
                cached_best_ask_ = &ask_data_[idx];
            }
            if (!cached_best_ask_) cached_best_ask_ = &ask_data_[idx];
        }
        ask_data_[idx].add_order(order);
    }

#if TITANIUM_ENABLE_PROFILING
    profile_stats_.add_ask_calls++;
    profile_stats_.add_ask_ns += (now_ns() - start_ns);
#endif
}

}  // namespace titanium
