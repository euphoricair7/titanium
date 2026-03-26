#pragma once

#include <atomic>
#include <array>
#include <cstddef>
#include <optional>

namespace titanium {

/**
 * @brief A lock-free Single-Producer/Single-Consumer (SPSC) queue.
 * 
 * This queue is designed for high-performance communication between two threads.
 * It uses a fixed-size ring buffer and atomic pointers with acquire/release semantics.
 * Cache-line alignment is used to prevent false sharing.
 * 
 * @tparam T The type of elements to store.
 * @tparam Capacity The maximum number of elements (must be a power of 2 for performance).
 */
template <typename T, size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

public:
    SPSCQueue() : head_(0), tail_(0) {}

    /**
     * @brief Tries to push an item into the queue.
     * 
     * @param item The item to push.
     * @return true if successful, false if the queue is full.
     */
    bool push(const T& item) {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (tail + 1) & mask_;

        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Queue is full
        }

        buffer_[tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    /**
     * @brief Tries to pop an item from the queue.
     * 
     * @return std::optional<T> The item if successful, std::nullopt if empty.
     */
    std::optional<T> pop() {
        const size_t head = head_.load(std::memory_order_relaxed);

        if (head == tail_.load(std::memory_order_acquire)) {
            return std::nullopt; // Queue is empty
        }

        T item = buffer_[head];
        head_.store((head + 1) & mask_, std::memory_order_release);
        return item;
    }

    /**
     * @brief Checks if the queue is empty.
     */
    bool empty() const {
        return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_relaxed);
    }

private:
    static constexpr size_t mask_ = Capacity - 1;

    // Pad indices to separate cache lines (typically 64 bytes) to avoid false sharing
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
    
    // The data buffer
    std::array<T, Capacity> buffer_;
};

} // namespace titanium
