#pragma once
#include <mutex>
#include <map>
#include <vector>
#include <atomic>
#include "order.h"

// Simple spinlock for MinGW environments without POSIX thread support
class Spinlock {
    std::atomic_flag flag = ATOMIC_FLAG_INIT;
public:
    void lock() {
        while (flag.test_and_set(std::memory_order_acquire)) {
        }
    }
    void unlock() {
        flag.clear(std::memory_order_release);
    }
};

class BaselineEngine {
public:
    void process_order(Order order);

private:
    Spinlock engine_mutex_;
    
    // Bids (Buys): Highest price first
    std::map<uint32_t, std::vector<Order>, std::greater<uint32_t>> bids_;
    // Asks (Sells): Lowest price first
    std::map<uint32_t, std::vector<Order>, std::less<uint32_t>> asks_;
};