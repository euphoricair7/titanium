#include "titanium/utils/order_generator.hpp"
#include <random>
#include <chrono>

namespace titanium {

std::vector<Order> generate_dummy_orders(std::size_t count) {
    std::vector<Order> orders;
    orders.reserve(count);

    std::mt19937 rng(42); 

    std::uniform_int_distribution<uint32_t> price_dist(90, 110);
    std::uniform_int_distribution<uint32_t> qty_dist(1, 100);
    std::uniform_int_distribution<int> side_dist(0, 1);

    // USE A FIXED BASE TIMESTAMP FOR DETERMINISM (Rollback)
    uint64_t now = 1712660000000000000ULL; 

    for (std::size_t i = 0; i < count; ++i) {
        Order order{};
        order.id = static_cast<uint64_t>(i + 1);
        order.side = static_cast<Side>(side_dist(rng));
        order.price = price_dist(rng);
        order.quantity = qty_dist(rng);
        order.timestamp = static_cast<uint64_t>(now + i);
        
        orders.push_back(std::move(order));
    }

    return orders;
}

} // namespace titanium
