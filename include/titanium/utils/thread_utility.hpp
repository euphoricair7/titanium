#pragma once

#include <pthread.h>
#include <iostream>

namespace titanium {
namespace utils {

/**
 * @brief Pins the current thread to a specific CPU core.
 * 
 * Crucial for reducing jitter and ensuring consistent L1/L2 cache locality
 * in high-performance engines.
 */
inline bool pin_thread_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    int result = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
    
    if (result != 0) {
        std::cerr << "Warning: Failed to pin thread to core " << core_id << std::endl;
        return false;
    }
    return true;
}

} // namespace utils
} // namespace titanium
