#pragma once

#include <mutex>
#include <map>
#include <vector>
#include <atomic>
#include "titanium/order.hpp"

namespace titanium {

// Simple spinlock for high-performance locking in the baseline engine
class Spinlock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
public:
    void lock() {
        while (flag.test_and_set(std::memory_order_acquire)) {
            // Adaptive backoff or yield could be added here for better performance
            #if defined(__i386__) || defined(__x86_64__)
                asm volatile("pause" ::: "memory");
            #endif
        }
    }
    void unlock() {
        flag.clear(std::memory_order_release);
    }
};

class BaselineEngine {
public:
    void process_order(Order order);

    // For verification/testing
    size_t get_bid_count() const { return bids_.size(); }
    size_t get_ask_count() const { return asks_.size(); }

private:
    Spinlock engine_mutex_;
    
    // Bids (Buys): Highest price first
    std::map<uint32_t, std::vector<Order>, std::greater<uint32_t>> bids_;
    // Asks (Sells): Lowest price first
    std::map<uint32_t, std::vector<Order>, std::less<uint32_t>> asks_;
};

} // namespace titanium
