#include "titanium/engine/titanium_engine.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>

#if defined(_MSC_VER) && defined(_WIN32)
#include <intrin.h>
#endif

#include "titanium/engine/risk_kernel.cuh"

namespace titanium {
namespace {
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

int find_highest_active_index(const std::bitset<TitaniumEngine::PRICE_WINDOW_SIZE>& bits) {
#if defined(_MSC_VER) && defined(_WIN32)
    const auto* words = reinterpret_cast<const unsigned __int64*>(&bits);
    for (std::size_t word = kActiveWordCount; word-- > 0;) {
        unsigned long bit = 0;
        if (_BitScanReverse64(&bit, words[word])) {
            return static_cast<int>(word * kWordBits + bit);
        }
    }
    return -1;
#else
    for (int i = static_cast<int>(TitaniumEngine::PRICE_WINDOW_SIZE) - 1; i >= 0; --i) {
        if (bits.test(static_cast<std::size_t>(i))) {
            return i;
        }
    }
    return -1;
#endif
}

int find_lowest_active_index(const std::bitset<TitaniumEngine::PRICE_WINDOW_SIZE>& bits) {
#if defined(_MSC_VER) && defined(_WIN32)
    const auto* words = reinterpret_cast<const unsigned __int64*>(&bits);
    for (std::size_t word = 0; word < kActiveWordCount; ++word) {
        unsigned long bit = 0;
        if (_BitScanForward64(&bit, words[word])) {
            return static_cast<int>(word * kWordBits + bit);
        }
    }
    return -1;
#else
    for (std::size_t i = 0; i < TitaniumEngine::PRICE_WINDOW_SIZE; ++i) {
        if (bits.test(i)) {
            return static_cast<int>(i);
        }
    }
    return -1;
#endif
}
}

TitaniumEngine::TitaniumEngine() {
    order_metadata_.resize(ORDER_TRACKER_CAP);
    bid_data_.resize(PRICE_WINDOW_SIZE);
    ask_data_.resize(PRICE_WINDOW_SIZE);
    bid_active_.resize(PRICE_WINDOW_SIZE, 0);
    ask_active_.resize(PRICE_WINDOW_SIZE, 0);
    overflow_order_tracker_.reserve(1'000'000);
}

TitaniumEngine::~TitaniumEngine() {
    if (pinned_orders_[0]) free_pinned_orders(pinned_orders_[0]);
    if (pinned_orders_[1]) free_pinned_orders(pinned_orders_[1]);
    if (pinned_results_[0]) free_pinned_results(pinned_results_[0]);
    if (pinned_results_[1]) free_pinned_results(pinned_results_[1]);
}

void TitaniumEngine::ensure_gpu_pipeline() {
    if (!async_risk_engine_) {
        async_risk_engine_ = std::make_unique<AsyncRiskEngine>(BATCH_SIZE);
    }
    if (!pinned_orders_[0]) pinned_orders_[0] = alloc_pinned_orders(BATCH_SIZE);
    if (!pinned_orders_[1]) pinned_orders_[1] = alloc_pinned_orders(BATCH_SIZE);
    if (!pinned_results_[0]) pinned_results_[0] = alloc_pinned_results(BATCH_SIZE);
    if (!pinned_results_[1]) pinned_results_[1] = alloc_pinned_results(BATCH_SIZE);
}

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
        return true;
    }
    return false;
}

bool TitaniumEngine::ensure_ask_window(uint32_t price) {
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
        return true;
    }
    return false;
}

void TitaniumEngine::update_best_bid_after_removal(std::size_t removed_idx) {
    if (best_bid_index_ != static_cast<int>(removed_idx)) return;
    best_bid_index_ = find_highest_active_index(bid_active_levels_);
}

void TitaniumEngine::update_best_ask_after_removal(std::size_t removed_idx) {
    if (best_ask_index_ != static_cast<int>(removed_idx)) return;
    best_ask_index_ = find_lowest_active_index(ask_active_levels_);
}

void TitaniumEngine::clear_bid_level(std::size_t idx) {
    if (!bid_active_[idx]) return;
    bid_active_[idx] = 0;
    bid_active_levels_.reset(idx);
    bid_data_[idx] = PriceLevelData{};
    bid_window_level_count_--;
    bid_count_--;
    update_best_bid_after_removal(idx);
}

void TitaniumEngine::clear_ask_level(std::size_t idx) {
    if (!ask_active_[idx]) return;
    ask_active_[idx] = 0;
    ask_active_levels_.reset(idx);
    ask_data_[idx] = PriceLevelData{};
    ask_window_level_count_--;
    ask_count_--;
    update_best_ask_after_removal(idx);
}

PriceLevelData* TitaniumEngine::best_bid_level(uint32_t& price) {
    PriceLevelData* window_level = nullptr;
    uint32_t window_price = 0;

    if (bid_window_level_count_ > 0) {
        if (best_bid_index_ < 0 ||
            !bid_active_[static_cast<std::size_t>(best_bid_index_)]) {
            best_bid_index_ = find_highest_active_index(bid_active_levels_);
        }

        if (best_bid_index_ >= 0) {
            window_price = bid_base_price_ +
                static_cast<uint32_t>(best_bid_index_);
            window_level = &bid_data_[static_cast<std::size_t>(best_bid_index_)];
        }
    }

    if (!overflow_bids_.empty()) {
        auto it = overflow_bids_.begin();
        if (!window_level || it->first > window_price) {
            price = it->first;
            return &it->second;
        }
    }

    if (window_level) {
        price = window_price;
        return window_level;
    }
    return nullptr;
}

PriceLevelData* TitaniumEngine::best_ask_level(uint32_t& price) {
    PriceLevelData* window_level = nullptr;
    uint32_t window_price = 0;

    if (ask_window_level_count_ > 0) {
        if (best_ask_index_ < 0 ||
            !ask_active_[static_cast<std::size_t>(best_ask_index_)]) {
            best_ask_index_ = find_lowest_active_index(ask_active_levels_);
        }

        if (best_ask_index_ >= 0) {
            window_price = ask_base_price_ +
                static_cast<uint32_t>(best_ask_index_);
            window_level = &ask_data_[static_cast<std::size_t>(best_ask_index_)];
        }
    }

    if (!overflow_asks_.empty()) {
        auto it = overflow_asks_.begin();
        if (!window_level || it->first < window_price) {
            price = it->first;
            return &it->second;
        }
    }

    if (window_level) {
        price = window_price;
        return window_level;
    }
    return nullptr;
}

void TitaniumEngine::track_order(uint64_t order_id, Side side, uint32_t price) {
    if (order_id < ORDER_TRACKER_CAP) {
        auto& meta = order_metadata_[static_cast<std::size_t>(order_id)];
        if (!meta.active) tracker_size_++;
        meta.active = true;
        meta.side = side;
        meta.price = price;
        return;
    }

    auto [it, inserted] = overflow_order_tracker_.insert_or_assign(
        order_id, OrderLocation{side, price});
    static_cast<void>(it);
    if (inserted) tracker_size_++;
}

void TitaniumEngine::untrack_order(uint64_t order_id) {
    if (order_id < ORDER_TRACKER_CAP) {
        auto& meta = order_metadata_[static_cast<std::size_t>(order_id)];
        if (meta.active) {
            meta.active = false;
            tracker_size_--;
        }
        return;
    }

    auto erased = overflow_order_tracker_.erase(order_id);
    if (erased > 0) tracker_size_--;
}

bool TitaniumEngine::lookup_order(uint64_t order_id, OrderLocation& out) const {
    if (order_id < ORDER_TRACKER_CAP) {
        const auto& meta = order_metadata_[static_cast<std::size_t>(order_id)];
        if (!meta.active) return false;
        out = OrderLocation{meta.side, meta.price};
        return true;
    }

    auto it = overflow_order_tracker_.find(order_id);
    if (it == overflow_order_tracker_.end()) return false;
    out = it->second;
    return true;
}

PriceLevelData* TitaniumEngine::find_level(Side side, uint32_t price) {
    if (side == Side::Buy) {
        if (price_in_bid_window(price)) {
            std::size_t idx = bid_index(price);
            if (bid_active_[idx]) return &bid_data_[idx];
            return nullptr;
        }
        auto it = overflow_bids_.find(price);
        return (it == overflow_bids_.end()) ? nullptr : &it->second;
    }

    if (price_in_ask_window(price)) {
        std::size_t idx = ask_index(price);
        if (ask_active_[idx]) return &ask_data_[idx];
        return nullptr;
    }
    auto it = overflow_asks_.find(price);
    return (it == overflow_asks_.end()) ? nullptr : &it->second;
}

void TitaniumEngine::process_order(Order order) {
    const auto start_ns = now_ns();
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
    profile_stats_.process_order_calls++;
    profile_stats_.process_order_ns += (now_ns() - start_ns);
}

void TitaniumEngine::process_orders_batched(const Order* orders,
                                            std::size_t total_count) {
    const auto batched_start_ns = now_ns();
    ensure_gpu_pipeline();

    std::size_t offset = 0;
    int current_buffer = 0;

    std::size_t current_batch = std::min(BATCH_SIZE, total_count);
    if (current_batch > 0) {
        auto memcpy_start = now_ns();
        std::memcpy(pinned_orders_[0], orders, current_batch * sizeof(Order));
        profile_stats_.batched_memcpy_ns += (now_ns() - memcpy_start);
        auto submit_start = now_ns();
        async_risk_engine_->submit_batch(
            pinned_orders_[0], pinned_results_[0], current_batch);
        profile_stats_.batched_submit_ns += (now_ns() - submit_start);
    }
    offset += current_batch;

    while (offset < total_count) {
        int next_buffer = 1 - current_buffer;
        std::size_t next_batch = std::min(BATCH_SIZE, total_count - offset);

        auto memcpy_start = now_ns();
        std::memcpy(pinned_orders_[next_buffer], orders + offset,
                    next_batch * sizeof(Order));
        profile_stats_.batched_memcpy_ns += (now_ns() - memcpy_start);

        auto sync_start = now_ns();
        async_risk_engine_->synchronize();
        profile_stats_.batched_sync_ns += (now_ns() - sync_start);

        auto submit_start = now_ns();
        async_risk_engine_->submit_batch(
            pinned_orders_[next_buffer], pinned_results_[next_buffer], next_batch);
        profile_stats_.batched_submit_ns += (now_ns() - submit_start);

        auto cpu_start = now_ns();
        for (std::size_t i = 0; i < current_batch; ++i) {
            process_order(pinned_orders_[current_buffer][i]);
        }
        profile_stats_.batched_cpu_process_ns += (now_ns() - cpu_start);

        offset += next_batch;
        current_batch = next_batch;
        current_buffer = next_buffer;
    }

    auto sync_start = now_ns();
    async_risk_engine_->synchronize();
    profile_stats_.batched_sync_ns += (now_ns() - sync_start);

    auto cpu_start = now_ns();
    for (std::size_t i = 0; i < current_batch; ++i) {
        process_order(pinned_orders_[current_buffer][i]);
    }
    profile_stats_.batched_cpu_process_ns += (now_ns() - cpu_start);

    profile_stats_.batched_calls++;
    profile_stats_.batched_orders += total_count;
    profile_stats_.batched_total_ns += (now_ns() - batched_start_ns);
}

bool TitaniumEngine::cancel_order(uint64_t order_id) {
    OrderLocation loc{};
    if (!lookup_order(order_id, loc)) return false;

    PriceLevelData* level = find_level(loc.side, loc.price);
    if (!level) return false;

    for (std::uint32_t j = 0; j < level->order_count; ++j) {
        if (level->orders[j].id != order_id) continue;

        level->total_quantity -= level->orders[j].quantity;
        if (j + 1 < level->order_count) {
            std::memmove(&level->orders[j], &level->orders[j + 1],
                (level->order_count - j - 1) * sizeof(Order));
        }
        level->order_count--;
        untrack_order(order_id);

        if (level->order_count == 0) {
            if (loc.side == Side::Buy) {
                if (price_in_bid_window(loc.price)) {
                    clear_bid_level(bid_index(loc.price));
                } else if (overflow_bids_.erase(loc.price) > 0) {
                    bid_count_--;
                }
            } else {
                if (price_in_ask_window(loc.price)) {
                    clear_ask_level(ask_index(loc.price));
                } else if (overflow_asks_.erase(loc.price) > 0) {
                    ask_count_--;
                }
            }
        }
        return true;
    }
    return false;
}

void TitaniumEngine::match_buy(Order& order) {
    const auto start_ns = now_ns();
    while (order.quantity > 0) {
        uint32_t best_price = 0;
        PriceLevelData* level = best_ask_level(best_price);
        if (!level) break;
        if (order.type == OrderType::Limit && best_price > order.price) break;

        for (std::uint32_t j = 0; j < level->order_count && order.quantity > 0;) {
            Order& resting = level->orders[j];
            if (resting.quantity <= order.quantity) {
                order.quantity -= resting.quantity;
                level->total_quantity -= resting.quantity;
                untrack_order(resting.id);
                if (j + 1 < level->order_count) {
                    std::memmove(&level->orders[j], &level->orders[j + 1],
                        (level->order_count - j - 1) * sizeof(Order));
                }
                level->order_count--;
            } else {
                resting.quantity -= order.quantity;
                level->total_quantity -= order.quantity;
                order.quantity = 0;
            }
        }

        if (level->order_count == 0) {
            if (price_in_ask_window(best_price)) {
                clear_ask_level(ask_index(best_price));
            } else if (overflow_asks_.erase(best_price) > 0) {
                ask_count_--;
            }
        }
    }
    profile_stats_.match_buy_calls++;
    profile_stats_.match_buy_ns += (now_ns() - start_ns);
}

void TitaniumEngine::match_sell(Order& order) {
    const auto start_ns = now_ns();
    while (order.quantity > 0) {
        uint32_t best_price = 0;
        PriceLevelData* level = best_bid_level(best_price);
        if (!level) break;
        if (order.type == OrderType::Limit && best_price < order.price) break;

        for (std::uint32_t j = 0; j < level->order_count && order.quantity > 0;) {
            Order& resting = level->orders[j];
            if (resting.quantity <= order.quantity) {
                order.quantity -= resting.quantity;
                level->total_quantity -= resting.quantity;
                untrack_order(resting.id);
                if (j + 1 < level->order_count) {
                    std::memmove(&level->orders[j], &level->orders[j + 1],
                        (level->order_count - j - 1) * sizeof(Order));
                }
                level->order_count--;
            } else {
                resting.quantity -= order.quantity;
                level->total_quantity -= order.quantity;
                order.quantity = 0;
            }
        }

        if (level->order_count == 0) {
            if (price_in_bid_window(best_price)) {
                clear_bid_level(bid_index(best_price));
            } else if (overflow_bids_.erase(best_price) > 0) {
                bid_count_--;
            }
        }
    }
    profile_stats_.match_sell_calls++;
    profile_stats_.match_sell_ns += (now_ns() - start_ns);
}

void TitaniumEngine::add_to_bids(const Order& order) {
    const auto start_ns = now_ns();
    const auto search_start = now_ns();
    bool use_window = ensure_bid_window(order.price) && price_in_bid_window(order.price);
    profile_stats_.add_bid_search_ns += (now_ns() - search_start);

    auto tracker_start = now_ns();
    track_order(order.id, Side::Buy, order.price);
    profile_stats_.add_bid_tracker_ns += (now_ns() - tracker_start);

    if (use_window) {
        std::size_t idx = bid_index(order.price);
        if (!bid_active_[idx]) {
            bid_active_[idx] = 1;
            bid_active_levels_.set(idx);
            bid_data_[idx] = PriceLevelData{};
            bid_window_level_count_++;
            bid_count_++;
            if (best_bid_index_ < 0 || static_cast<int>(idx) > best_bid_index_) {
                best_bid_index_ = static_cast<int>(idx);
            }
        }
        bid_data_[idx].add_order(order);
    } else {
        auto it = overflow_bids_.find(order.price);
        if (it == overflow_bids_.end()) {
            it = overflow_bids_.emplace(order.price, PriceLevelData{}).first;
            bid_count_++;
        }
        it->second.add_order(order);
    }

    profile_stats_.add_bid_calls++;
    profile_stats_.add_bid_ns += (now_ns() - start_ns);
}

void TitaniumEngine::add_to_asks(const Order& order) {
    const auto start_ns = now_ns();
    const auto search_start = now_ns();
    bool use_window = ensure_ask_window(order.price) && price_in_ask_window(order.price);
    profile_stats_.add_ask_search_ns += (now_ns() - search_start);

    auto tracker_start = now_ns();
    track_order(order.id, Side::Sell, order.price);
    profile_stats_.add_ask_tracker_ns += (now_ns() - tracker_start);

    if (use_window) {
        std::size_t idx = ask_index(order.price);
        if (!ask_active_[idx]) {
            ask_active_[idx] = 1;
            ask_active_levels_.set(idx);
            ask_data_[idx] = PriceLevelData{};
            ask_window_level_count_++;
            ask_count_++;
            if (best_ask_index_ < 0 || static_cast<int>(idx) < best_ask_index_) {
                best_ask_index_ = static_cast<int>(idx);
            }
        }
        ask_data_[idx].add_order(order);
    } else {
        auto it = overflow_asks_.find(order.price);
        if (it == overflow_asks_.end()) {
            it = overflow_asks_.emplace(order.price, PriceLevelData{}).first;
            ask_count_++;
        }
        it->second.add_order(order);
    }

    profile_stats_.add_ask_calls++;
    profile_stats_.add_ask_ns += (now_ns() - start_ns);
}

}  // namespace titanium
