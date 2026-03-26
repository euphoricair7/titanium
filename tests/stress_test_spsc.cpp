#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <chrono>
#include "titanium/concurrency/spsc_queue.hpp"
#include "titanium/order.hpp"

using namespace titanium;

const size_t NUM_ORDERS = 10'000'000;
const size_t QUEUE_CAPACITY = 1024 * 64; // Power of 2

void stress_test() {
    SPSCQueue<Order, QUEUE_CAPACITY> queue;
    std::vector<Order> sent_orders;
    sent_orders.reserve(NUM_ORDERS);

    std::cout << "Starting SPSC Stress Test: 10 Million Orders..." << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();

    // Producer Thread
    std::thread producer([&]() {
        for (size_t i = 0; i < NUM_ORDERS; ++i) {
            Order order{.id = i, .timestamp = i, .next = 0, .price = 100, .quantity = 1, .side = Side::Buy};
            while (!queue.push(order)) {
                // Spin-wait if full
                #if defined(__i386__) || defined(__x86_64__)
                    asm volatile("pause" ::: "memory");
                #endif
            }
        }
    });

    // Consumer Thread
    std::thread consumer([&]() {
        size_t received_count = 0;
        while (received_count < NUM_ORDERS) {
            auto order = queue.pop();
            if (order) {
                assert(order->id == received_count);
                received_count++;
            } else {
                // Spin-wait if empty
                #if defined(__i386__) || defined(__x86_64__)
                    asm volatile("pause" ::: "memory");
                #endif
            }
        }
    });

    producer.join();
    consumer.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;

    std::cout << "Stress Test PASSED!" << std::endl;
    std::cout << "Elapsed Time: " << elapsed.count() << "s" << std::endl;
    std::cout << "Throughput: " << (NUM_ORDERS / elapsed.count()) / 1e6 << " million orders/sec" << std::endl;
}

int main() {
    try {
        stress_test();
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
