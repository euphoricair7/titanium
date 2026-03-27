#include "titanium/engine/titanium_engine.hpp"
#include <algorithm>
#include <cstring>
#include <immintrin.h>

namespace titanium {

void TitaniumEngine::process_order(Order order) {
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
}

bool TitaniumEngine::cancel_order(uint64_t order_id) {
    auto it = order_tracker_.find(order_id);
    if (it == order_tracker_.end()) return false;

    auto& loc = it->second;
    auto& prices = (loc.side == Side::Buy) ? bid_prices_ : ask_prices_;
    auto& data = (loc.side == Side::Buy) ? bid_data_ : ask_data_;
    std::size_t& count = (loc.side == Side::Buy) ? bid_count_ : ask_count_;

    for (std::size_t i = 0; i < count; ++i) {
        if (prices[i] == loc.price) {
            PriceLevelData& level = data[i];
            for (std::uint32_t j = 0; j < level.order_count; ++j) {
                if (level.orders[j].id == order_id) {
                    level.total_quantity -= level.orders[j].quantity;
                    
                    if (j < level.order_count - 1) {
                        std::memmove(&level.orders[j], &level.orders[j+1], (level.order_count - j - 1) * sizeof(Order));
                    }
                    level.order_count--;

                    if (level.order_count == 0) {
                        if (i < count - 1) {
                            std::memmove(&prices[i], &prices[i+1], (count - i - 1) * sizeof(uint32_t));
                            std::memmove(&data[i], &data[i+1], (count - i - 1) * sizeof(PriceLevelData));
                        }
                        count--;
                    }
                    
                    order_tracker_.erase(it);
                    return true;
                }
            }
        }
    }
    return false;
}

void TitaniumEngine::match_buy(Order& order) {
    // SIMD Price Search would move market order matching forward
    // For now, keep it simple: Market vs Best Ask
    for (std::size_t i = 0; i < ask_count_ && order.quantity > 0; ) {
        if (order.type == OrderType::Limit && ask_prices_[i] > order.price) break;

        PriceLevelData& level = ask_data_[i];
        for (std::uint32_t j = 0; j < level.order_count && order.quantity > 0; ) {
            Order& book_order = level.orders[j];
            if (book_order.quantity <= order.quantity) {
                order.quantity -= book_order.quantity;
                level.total_quantity -= book_order.quantity;
                order_tracker_.erase(book_order.id);

                if (j < level.order_count - 1) {
                    std::memmove(&level.orders[j], &level.orders[j+1], (level.order_count - j - 1) * sizeof(Order));
                }
                level.order_count--;
            } else {
                book_order.quantity -= order.quantity;
                level.total_quantity -= order.quantity;
                order.quantity = 0;
                j++;
            }
        }

        if (level.order_count == 0) {
            if (i < ask_count_ - 1) {
                std::memmove(&ask_prices_[i], &ask_prices_[i+1], (ask_count_ - i - 1) * sizeof(uint32_t));
                std::memmove(&ask_data_[i], &ask_data_[i+1], (ask_count_ - i - 1) * sizeof(PriceLevelData));
            }
            ask_count_--;
        } else {
            i++;
        }
    }
}

void TitaniumEngine::match_sell(Order& order) {
    for (std::size_t i = 0; i < bid_count_ && order.quantity > 0; ) {
        if (order.type == OrderType::Limit && bid_prices_[i] < order.price) break;

        PriceLevelData& level = bid_data_[i];
        for (std::uint32_t j = 0; j < level.order_count && order.quantity > 0; ) {
            Order& book_order = level.orders[j];
            if (book_order.quantity <= order.quantity) {
                order.quantity -= book_order.quantity;
                level.total_quantity -= book_order.quantity;
                order_tracker_.erase(book_order.id);

                if (j < level.order_count - 1) {
                    std::memmove(&level.orders[j], &level.orders[j+1], (level.order_count - j - 1) * sizeof(Order));
                }
                level.order_count--;
            } else {
                book_order.quantity -= order.quantity;
                level.total_quantity -= order.quantity;
                order.quantity = 0;
                j++;
            }
        }

        if (level.order_count == 0) {
            if (i < bid_count_ - 1) {
                std::memmove(&bid_prices_[i], &bid_prices_[i+1], (bid_count_ - i - 1) * sizeof(uint32_t));
                std::memmove(&bid_data_[i], &bid_data_[i+1], (bid_count_ - i - 1) * sizeof(PriceLevelData));
            }
            bid_count_--;
        } else {
            i++;
        }
    }
}

void TitaniumEngine::add_to_bids(const Order& order) {
    std::size_t i = 0;
    // SIMD Price Search (Flex!)
    // If the book is deep, we scan 8 prices at a time
    if (bid_count_ >= 8) {
        __m256i order_price_v = _mm256_set1_epi32(order.price);
        for (; i + 7 < bid_count_; i += 8) {
            __m256i prices_v = _mm256_load_si256((const __m256i*)&bid_prices_[i]);
            // Search for first price < order_price
            __m256i cmp_result = _mm256_cmpgt_epi32(order_price_v, prices_v); 
            int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp_result));
            if (mask != 0) {
                i += __builtin_ctz(mask);
                goto insert_bid;
            }
            
            // Check for equality (can also be done with SIMD but simpler for now)
            for (std::size_t k = 0; k < 8; ++k) {
                if (bid_prices_[i+k] == order.price) {
                    i += k;
                    goto existing_bid;
                }
            }
        }
    }

    // Residual loop
    for (; i < bid_count_; ++i) {
        if (bid_prices_[i] == order.price) goto existing_bid;
        if (bid_prices_[i] < order.price) break;
    }

insert_bid:
    if (bid_count_ < MAX_LEVELS) {
        if (i < bid_count_) {
            std::memmove(&bid_prices_[i+1], &bid_prices_[i], (bid_count_ - i) * sizeof(uint32_t));
            std::memmove(&bid_data_[i+1], &bid_data_[i], (bid_count_ - i) * sizeof(PriceLevelData));
        }
        bid_prices_[i] = order.price;
        bid_data_[i] = PriceLevelData{};
        bid_data_[i].add_order(order);
        order_tracker_[order.id] = {Side::Buy, order.price};
        bid_count_++;
    }
    return;

existing_bid:
    bid_data_[i].add_order(order);
    order_tracker_[order.id] = {Side::Buy, order.price};
}

void TitaniumEngine::add_to_asks(const Order& order) {
    std::size_t i = 0;
    // SIMD Price Search for Asks
    if (ask_count_ >= 8) {
        __m256i order_price_v = _mm256_set1_epi32(order.price);
        for (; i + 7 < ask_count_; i += 8) {
            __m256i prices_v = _mm256_load_si256((const __m256i*)&ask_prices_[i]);
            // Search for first price > order_price
            __m256i cmp_result = _mm256_cmpgt_epi32(prices_v, order_price_v); 
            int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmp_result));
            if (mask != 0) {
                i += __builtin_ctz(mask);
                goto insert_ask;
            }

            for (std::size_t k = 0; k < 8; ++k) {
                if (ask_prices_[i+k] == order.price) {
                    i += k;
                    goto existing_ask;
                }
            }
        }
    }

    for (; i < ask_count_; ++i) {
        if (ask_prices_[i] == order.price) goto existing_ask;
        if (ask_prices_[i] > order.price) break;
    }

insert_ask:
    if (ask_count_ < MAX_LEVELS) {
        if (i < ask_count_) {
            std::memmove(&ask_prices_[i+1], &ask_prices_[i], (ask_count_ - i) * sizeof(uint32_t));
            std::memmove(&ask_data_[i+1], &ask_data_[i], (ask_count_ - i) * sizeof(PriceLevelData));
        }
        ask_prices_[i] = order.price;
        ask_data_[i] = PriceLevelData{};
        ask_data_[i].add_order(order);
        order_tracker_[order.id] = {Side::Sell, order.price};
        ask_count_++;
    }
    return;

existing_ask:
    ask_data_[i].add_order(order);
    order_tracker_[order.id] = {Side::Sell, order.price};
}

} // namespace titanium
