#include <iostream>
#include <cassert>
#include "titanium/order.hpp"
#include "titanium/engine/baseline_engine.hpp"

using namespace titanium;

void test_order_size() {
    std::cout << "Testing Order size... ";
    assert(sizeof(Order) == 64);
    std::cout << "PASSED" << std::endl;
}

void test_basic_matching() {
    std::cout << "Testing basic matching... ";
    BaselineEngine engine;

    // Add a Sell order at 100
    Order sell1{.id = 1, .timestamp = 0, .next = 0, .price = 100, .quantity = 10, .side = Side::Sell};
    engine.process_order(sell1);

    // Add a Buy order at 100
    Order buy1{.id = 2, .timestamp = 0, .next = 0, .price = 100, .quantity = 10, .side = Side::Buy};
    engine.process_order(buy1);

    assert(engine.get_ask_count() == 0);
    assert(engine.get_bid_count() == 0);
    std::cout << "PASSED" << std::endl;
}

void test_partial_fill() {
    std::cout << "Testing partial fill... ";
    BaselineEngine engine;

    // Buy 20 at 100
    engine.process_order({.id = 1, .timestamp = 0, .next = 0, .price = 100, .quantity = 20, .side = Side::Buy});
    // Sell 15 at 100
    engine.process_order({.id = 2, .timestamp = 0, .next = 0, .price = 100, .quantity = 15, .side = Side::Sell});

    assert(engine.get_bid_count() == 1);
    assert(engine.get_ask_count() == 0);
    std::cout << "PASSED" << std::endl;
}

int main() {
    try {
        test_order_size();
        test_basic_matching();
        test_partial_fill();
        std::cout << "\nAll core tests PASSED!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}