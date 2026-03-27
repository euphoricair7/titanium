#pragma once

#include <cstdint>

namespace titanium {

enum class Side : uint8_t {
    Buy = 0,
    Sell = 1
};

enum class OrderType : uint8_t {
    Limit = 0,
    Market = 1
};

/**
 * @brief Core Order struct, 64-byte cache-line aligned.
 */
struct alignas(64) Order {
    uint64_t id;
    uint64_t timestamp;
    uint64_t next;
    uint32_t price;
    uint32_t quantity;
    Side side;
    OrderType type;
    char padding[64 - 36 - 1]; // Adjusted padding
};

static_assert(sizeof(Order) == 64, "Order must be 64 bytes to fit in a cache line");

} // namespace titanium