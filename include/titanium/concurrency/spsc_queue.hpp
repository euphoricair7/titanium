#pragma once

#include <atomic>
#include <algorithm>
#include <array>
#include <cstddef>
#include <new>
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
    static constexpr std::size_t cache_line_size_ =
#ifdef __cpp_lib_hardware_interference_size
        std::hardware_destructive_interference_size;
#else
        64;
#endif

    struct alignas(cache_line_size_) PaddedAtomicSizeT {
        std::atomic<size_t> value{0};
        static constexpr std::size_t kPadding =
            cache_line_size_ > sizeof(std::atomic<size_t>)
                ? cache_line_size_ - sizeof(std::atomic<size_t>)
                : 1;
        std::array<std::byte, kPadding> padding{};
    };

public:
    SPSCQueue() = default;

    /**
     * @brief Tries to push an item into the queue.
     * 
     * @param item The item to push.
     * @return true if successful, false if the queue is full.
     */
    bool push(const T& item) {
        return try_push_batch(&item, 1) == 1;
    }

    /**
     * @brief Pushes up to count items with one tail publish.
     *
     * @param items Pointer to source items.
     * @param count Number of items to attempt.
     * @return Number of items actually pushed.
     */
    size_t try_push_batch(const T* items, size_t count) {
        if (count == 0) return 0;

        const size_t tail = tail_.value.load(std::memory_order_relaxed);
        const size_t head = head_.value.load(std::memory_order_acquire);
        const size_t used = (tail + Capacity - head) & mask_;
        const size_t free_slots = mask_ - used;
        const size_t to_push = std::min(count, free_slots);
        if (to_push == 0) return 0;

        for (size_t i = 0; i < to_push; ++i) {
            buffer_[(tail + i) & mask_] = items[i];
        }

        tail_.value.store((tail + to_push) & mask_, std::memory_order_release);
        return to_push;
    }

    /**
     * @brief Tries to pop an item from the queue.
     * 
     * @return std::optional<T> The item if successful, std::nullopt if empty.
     */
    std::optional<T> pop() {
        T item;
        if (pop_batch(&item, 1) == 0) {
            return std::nullopt;
        }
        return item;
    }

    /**
     * @brief Pops up to max_count items with one head publish.
     *
     * @param out Pointer to destination buffer.
     * @param max_count Maximum items to pop.
     * @return Number of items actually popped.
     */
    size_t pop_batch(T* out, size_t max_count) {
        if (max_count == 0) return 0;

        const size_t head = head_.value.load(std::memory_order_relaxed);
        const size_t tail = tail_.value.load(std::memory_order_acquire);
        const size_t available = (tail + Capacity - head) & mask_;
        const size_t to_pop = std::min(max_count, available);
        if (to_pop == 0) return 0;

        for (size_t i = 0; i < to_pop; ++i) {
            out[i] = buffer_[(head + i) & mask_];
        }

        head_.value.store((head + to_pop) & mask_, std::memory_order_release);
        return to_pop;
    }

    /**
     * @brief Checks if the queue is empty.
     */
    bool empty() const {
        return head_.value.load(std::memory_order_relaxed) ==
               tail_.value.load(std::memory_order_relaxed);
    }

private:
    static constexpr size_t mask_ = Capacity - 1;

    // Separate cache lines to avoid producer/consumer false sharing.
    PaddedAtomicSizeT head_;
    PaddedAtomicSizeT tail_;
    
    // The data buffer
    std::array<T, Capacity> buffer_;
};

} // namespace titanium
