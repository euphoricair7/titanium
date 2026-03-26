#include "titanium/engine/titanium_engine.hpp"
#include <algorithm>
#include <cstring>

namespace titanium {

void TitaniumEngine::process_order(Order order) {
    if (order.side == Side::Buy) {
        match_buy(order);
        if (order.quantity > 0) {
            add_to_bids(order);
        }
    } else {
        match_sell(order);
        if (order.quantity > 0) {
            add_to_asks(order);
        }
    }
}

void TitaniumEngine::match_buy(Order& order) {
    for (uint32_t i = 0; i < ask_count_ && order.quantity > 0; ) {
        PriceLevel& level = asks_[i];
        if (level.price > order.price) break;

        for (uint32_t j = 0; j < level.order_count && order.quantity > 0; ) {
            Order& book_order = level.orders[j];
            if (book_order.quantity <= order.quantity) {
                order.quantity -= book_order.quantity;
                level.total_quantity -= book_order.quantity;
                
                // Remove order from level (shift array)
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
            // Remove level from asks
            if (i < ask_count_ - 1) {
                std::memmove(&asks_[i], &asks_[i+1], (ask_count_ - i - 1) * sizeof(PriceLevel));
            }
            ask_count_--;
        } else {
            i++;
        }
    }
}

void TitaniumEngine::match_sell(Order& order) {
    for (uint32_t i = 0; i < bid_count_ && order.quantity > 0; ) {
        PriceLevel& level = bids_[i];
        if (level.price < order.price) break;

        for (uint32_t j = 0; j < level.order_count && order.quantity > 0; ) {
            Order& book_order = level.orders[j];
            if (book_order.quantity <= order.quantity) {
                order.quantity -= book_order.quantity;
                level.total_quantity -= book_order.quantity;
                
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
                std::memmove(&bids_[i], &bids_[i+1], (bid_count_ - i - 1) * sizeof(PriceLevel));
            }
            bid_count_--;
        } else {
            i++;
        }
    }
}

void TitaniumEngine::add_to_bids(const Order& order) {
    uint32_t i = 0;
    for (; i < bid_count_; ++i) {
        if (bids_[i].price == order.price) {
            bids_[i].add_order(order);
            return;
        }
        if (bids_[i].price < order.price) break;
    }

    if (bid_count_ < TitaniumEngine::MAX_LEVELS) {
        if (i < bid_count_) {
            std::memmove(&bids_[i+1], &bids_[i], (bid_count_ - i) * sizeof(PriceLevel));
        }
        bids_[i] = PriceLevel{order.price, 0};
        bids_[i].add_order(order);
        bid_count_++;
    }
}

void TitaniumEngine::add_to_asks(const Order& order) {
    uint32_t i = 0;
    for (; i < ask_count_; ++i) {
        if (asks_[i].price == order.price) {
            asks_[i].add_order(order);
            return;
        }
        if (asks_[i].price > order.price) break;
    }

    if (ask_count_ < TitaniumEngine::MAX_LEVELS) {
        if (i < ask_count_) {
            std::memmove(&asks_[i+1], &asks_[i], (ask_count_ - i) * sizeof(PriceLevel));
        }
        asks_[i] = PriceLevel{order.price, 0};
        asks_[i].add_order(order);
        ask_count_++;
    }
}

} // namespace titanium
