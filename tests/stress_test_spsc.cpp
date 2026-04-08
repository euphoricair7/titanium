#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <memory>
#include "titanium/concurrency/spsc_queue.hpp"
#include "titanium/order.hpp"
#include "titanium/utils/thread_utility.hpp"

using namespace titanium;

void run_stress_test(size_t num_orders) {
    auto queue = std::make_unique<SPSCQueue<Order, 65536>>();
    std::atomic<bool> done{false};
    std::atomic<size_t> consumed_count{0};

    std::cout << "Starting SPSC Stress Test: " << (num_orders / 1000000.0) << " Million Orders..." << std::endl;
    if (num_orders > 0) std::cout << "Using Thread Pinning (Producer: Core 1, Consumer: Core 2)" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    // Consumer Thread (Matching Engine Ingestion)
    std::thread consumer([&]() {
        utils::pin_thread_to_core(2); // Pin to Core 2
        while (!done || !queue->empty()) {
            auto order_opt = queue->pop();
            if (order_opt) {
                consumed_count++;
            }
        }
    });

    // Producer Thread (Gateway Ingestion)
    std::thread producer([&]() {
        utils::pin_thread_to_core(1); // Pin to Core 1
        for (size_t i = 0; i < num_orders; ++i) {
            Order order{.id = i, .timestamp = i, .next = 0, .price = 100, .quantity = 1, .side = Side::Buy, .type = OrderType::Limit};
            while (!queue->push(order)) {
                // Spinning is faster than yielding for high-throughput queues
            }
        }
        done = true;
    });

    producer.join();
    consumer.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    if (consumed_count == num_orders) {
        std::cout << "Stress Test PASSED!" << std::endl;
        std::cout << "Elapsed Time: " << elapsed.count() << "s" << std::endl;
        std::cout << "Throughput: " << (num_orders / elapsed.count() / 1000000.0) << " million orders/sec" << std::endl;
    } else {
        std::cout << "Stress Test FAILED! Consumed " << consumed_count << " / " << num_orders << std::endl;
    }
}

int main() {
    run_stress_test(10000000); // 10 Million orders
    return 0;
}
