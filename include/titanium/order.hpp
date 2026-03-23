#pragma once

#include <cstdint>
#include <atomic>

namespace titanium {

struct alignas(64) Order {
    uint64_t order_id;     // 8
    uint64_t price;        // 8
    uint32_t quantity;     // 4
    uint32_t side;         // 4

    uint64_t timestamp;    // 8
    std::atomic<uint64_t> next; // 8

    char padding[64 - (8 + 8 + 4 + 4 + 8 + 8)];
};

static_assert(sizeof(Order) == 64, "Order must be exactly 64 bytes");

} // namespace titanium