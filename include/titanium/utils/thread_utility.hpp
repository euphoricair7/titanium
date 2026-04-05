#pragma once

#include <iostream>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace titanium {
namespace utils {

/**
 * @brief Pins the current thread to a specific CPU core.
 * 
 * Crucial for reducing jitter and ensuring consistent L1/L2 cache locality
 * in high-performance engines.
 */
inline bool pin_thread_to_core(int core_id) {
#ifdef _WIN32
    HANDLE thread = GetCurrentThread();
    DWORD_PTR mask = (1ULL << core_id);
    DWORD_PTR result = SetThreadAffinityMask(thread, mask);
    if (result == 0) {
        std::cerr << "Warning: Failed to pin thread to core " << core_id << " on Windows." << std::endl;
        return false;
    }
    return true;
#else
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
#endif
}

} // namespace utils
} // namespace titanium
