#pragma once
#include <cstdint>

struct Order {
    uint64_t id;
    bool is_buy;
    uint32_t price;
    uint32_t quantity;
};