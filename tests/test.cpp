#include <iostream>
#include <cassert>
#include "titanium/order.hpp"
#include "titanium/engine/baseline_engine.hpp"
#include "titanium/engine/titanium_engine.hpp"

using namespace titanium;

void test_order_size() {
    std::cout << "Testing Order size... ";
    assert(sizeof(Order) == 64);
    std::cout << "PASSED" << std::endl;
}

void test_basic_matching() {
    std::cout << "Testing basic matching... ";
    TitaniumEngine engine;

    engine.process_order({.id = 1, .timestamp = 0, .next = 0, .price = 100, .quantity = 10, .side = Side::Sell, .type = OrderType::Limit});
    engine.process_order({.id = 2, .timestamp = 0, .next = 0, .price = 100, .quantity = 10, .side = Side::Buy, .type = OrderType::Limit});

    assert(engine.get_ask_count() == 0);
    assert(engine.get_bid_count() == 0);
    std::cout << "PASSED" << std::endl;
}

void test_market_order() {
    std::cout << "Testing Market order... ";
    TitaniumEngine engine;

    // Add liquidity
    engine.process_order({.id = 1, .timestamp = 0, .next = 0, .price = 100, .quantity = 10, .side = Side::Sell, .type = OrderType::Limit});
    engine.process_order({.id = 2, .timestamp = 0, .next = 0, .price = 105, .quantity = 10, .side = Side::Sell, .type = OrderType::Limit});

    // Market Buy for 15 units
    // Should take 10 at 100 and 5 at 105
    engine.process_order({.id = 3, .timestamp = 0, .next = 0, .price = 0, .quantity = 15, .side = Side::Buy, .type = OrderType::Market});

    assert(engine.get_ask_count() == 1); // Only the remainder at 105
    // Market buy should NOT be in the book
    assert(engine.get_bid_count() == 0);

    std::cout << "PASSED" << std::endl;
}

void test_order_cancellation() {
    std::cout << "Testing order cancellation... ";
    TitaniumEngine engine;

    engine.process_order({.id = 101, .timestamp = 0, .next = 0, .price = 100, .quantity = 10, .side = Side::Buy, .type = OrderType::Limit});
    engine.process_order({.id = 102, .timestamp = 0, .next = 0, .price = 105, .quantity = 20, .side = Side::Sell, .type = OrderType::Limit});
    
    assert(engine.get_bid_count() == 1);
    assert(engine.get_ask_count() == 1);

    assert(engine.cancel_order(101) == true);
    assert(engine.get_bid_count() == 0);
    assert(engine.cancel_order(102) == true);
    assert(engine.get_ask_count() == 0);

    std::cout << "PASSED" << std::endl;
}

int main() {
    try {
        test_order_size();
        test_basic_matching();
        test_market_order();
        test_order_cancellation();
        std::cout << "\nAll core tests PASSED!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}