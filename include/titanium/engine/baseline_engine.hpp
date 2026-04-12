#pragma once

#include <unordered_map>
#include <deque>
#include <algorithm>
#include "titanium/order.hpp"

namespace titanium {

class BaselineEngine {
public:
    void process_order(Order order);

    size_t get_bid_count() const { return bids_.size(); }
    size_t get_ask_count() const { return asks_.size(); }

private:
    std::unordered_map<uint32_t, std::deque<Order>> bids_;
    std::unordered_map<uint32_t, std::deque<Order>> asks_;
};

} // namespace titanium