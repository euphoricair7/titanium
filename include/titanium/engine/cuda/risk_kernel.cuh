#pragma once

#include <array>
#include <cstddef>
#include "titanium/order.hpp"
#include <mutex>

struct CUstream_st;
struct CUgraph_st;
struct CUgraphExec_st;

namespace titanium {

/**
 * @brief CPU wrapper to execute a dummy risk kernel on the GPU.
 * 
 * @param orders Array of orders to process.
 * @param count Number of orders in the array.
 * @param results Output array where results will be stored. Must have at least `count` capacity.
 */
void run_dummy_risk_check(const Order* orders, std::size_t count, float* results);

// Host Memory Registration for zero-copy async transfers
void register_host_memory(void* ptr, std::size_t size);
void unregister_host_memory(void* ptr);

// Pinned memory allocation for async transfers
Order* alloc_pinned_orders(std::size_t count);
void free_pinned_orders(Order* ptr);
float* alloc_pinned_results(std::size_t count);
void free_pinned_results(float* ptr);

class AsyncRiskEngine {
public:
    AsyncRiskEngine(std::size_t batch_size);
    ~AsyncRiskEngine();

    // Queues a batch to the GPU asynchronously
    void submit_batch(const Order* host_orders, float* host_results, std::size_t count);

    // Sync physical stream or specific slot
    void synchronize(int slot_idx = -1);

private:
    struct GraphSlot {
        CUgraph_st* graph = nullptr;
        CUgraphExec_st* graph_exec = nullptr;
        void* event = nullptr; // Using void* to avoid including cuda_runtime.h in header if possible, or just use CUevent_st*
        bool is_captured = false;
        const Order* host_orders = nullptr;
        float* host_results = nullptr;
        std::size_t count = 0;
    };

    std::size_t max_batch_size_;
    CUstream_st* stream_;
    static constexpr std::size_t GRAPH_SLOT_COUNT = 4;
    std::array<GraphSlot, GRAPH_SLOT_COUNT> graph_slots_{};
    Order* d_orders_;
    float* d_results_;
    std::mutex mutex_;
};

} // namespace titanium
