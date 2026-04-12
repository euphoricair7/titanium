#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <memory>
#include "titanium/concurrency/spsc_queue.hpp"
#include "titanium/order.hpp"
#include "titanium/utils/thread_utility.hpp"

using namespace titanium;

bool run_stress_test(size_t num_orders) {
    // SPSCQueue<Order,65536> = 65536 * 64 bytes = 4MB.  Heap-allocate to avoid stack overflow.
    auto queue_ptr = std::make_unique<SPSCQueue<Order, 65536>>();
    auto& queue = *queue_ptr;
    std::atomic<bool> done{false};
    std::atomic<size_t> consumed_count{0};

    std::cout << "Starting SPSC Stress Test: " << (num_orders / 1000000.0) << " Million Orders..." << std::endl;
    std::cout.flush();

    auto start = std::chrono::high_resolution_clock::now();

    // Consumer Thread
    std::thread consumer([&]() {
        bool pinned = utils::pin_thread_to_core(2);
        if (!pinned) std::cerr << "[Consumer] Warning: thread pinning failed, continuing unpinned.\n";
        while (!done || !queue.empty()) {
            auto order_opt = queue.pop();
            if (order_opt) {
                consumed_count++;
            }
        }
    });

    // Producer Thread
    std::thread producer([&]() {
        bool pinned = utils::pin_thread_to_core(1);
        if (!pinned) std::cerr << "[Producer] Warning: thread pinning failed, continuing unpinned.\n";
        for (size_t i = 0; i < num_orders; ++i) {
            Order order{};
            order.id        = i;
            order.timestamp = i;
            order.next      = 0;
            order.price     = 100;
            order.quantity  = 1;
            order.side      = Side::Buy;
            order.type      = OrderType::Limit;
            while (!queue.push(order)) {}
        }
        done = true;
    });

    producer.join();
    consumer.join();

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;

    if (consumed_count == num_orders) {
        std::cout << "Stress Test PASSED! (" << consumed_count << "/" << num_orders << " orders)" << std::endl;
        std::cout << "Elapsed Time: " << elapsed.count() << "s" << std::endl;
        std::cout << "Throughput: " << (num_orders / elapsed.count() / 1000000.0) << " million orders/sec" << std::endl;
        std::cout.flush();
        return true;
    } else {
        std::cout << "Stress Test FAILED! Consumed " << consumed_count << " / " << num_orders << std::endl;
        std::cout.flush();
        return false;
    }
}

int main() {
    bool passed = run_stress_test(10000000); // 10 Million orders
    return passed ? 0 : 1;
}
