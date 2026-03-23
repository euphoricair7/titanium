#include "order_generator.h"
#include <random>

std::vector<Order> generate_dummy_orders(std::size_t count) {
    // Pre-allocate the vector with 'count' elements to avoid reallocation overhead.
    std::vector<Order> orders(count);

    // Fixed seed guarantees the sequence of orders is identical across every run.
    std::mt19937 rng(42); 

    std::uniform_int_distribution<uint32_t> price_dist(90, 110);
    std::uniform_int_distribution<uint32_t> qty_dist(1, 100);
    std::uniform_int_distribution<int> side_dist(0, 1);

    for (std::size_t i = 0; i < count; ++i) {
        orders[i] = {
            static_cast<uint64_t>(i + 1), 
            side_dist(rng) == 1,          
            price_dist(rng),
            qty_dist(rng)
        };
    }

    return orders;
}