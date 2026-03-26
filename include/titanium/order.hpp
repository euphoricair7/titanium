#pragma once

#include <cstdint>

namespace titanium {

enum class Side : uint32_t {
    Buy = 0,
    Sell = 1
};

struct alignas(64) Order {
    uint64_t id;           // 8 
    uint64_t timestamp;    // 8
    uint64_t next;         // 8 
    uint32_t price;        // 4 
    uint32_t quantity;     // 4
    Side side;             // 4
    
    // Total so far: 8 + 8 + 8 + 4 + 4 + 4 = 36 bytes.
    // To reach exactly 64 bytes, we need 28 bytes of padding.
    char padding[64 - 36]; 
};

static_assert(sizeof(Order) == 64, "Order must be exactly 64 bytes");

} // namespace titanium