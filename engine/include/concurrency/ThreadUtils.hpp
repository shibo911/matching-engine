#pragma once

#include <thread>
#include <iostream>
#include <system_error>

// Cross-Platform OS headers
#if defined(__linux__) || defined(__APPLE__)
    #include <pthread.h>
    #include <sched.h>
#elif defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

namespace matching_engine {
namespace concurrency {

// Pins the currently executing thread to a specific hardware CPU core.
//
// Mechanical Sympathy Note: 
// The OS scheduler loves to migrate threads across CPU cores to balance load.
// When a thread migrates, its L1/L2 cache is entirely invalidated, causing
// massive latency spikes while the cache warms up on the new core.
// By permanently pinning the Matching Engine thread to an isolated core,
// we ensure the Hot Path executes with 100% cache locality permanently,
// completely bypassing OS context-switching overhead.
inline bool pin_current_thread_to_core(int core_id) {
#if defined(__linux__)
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);

    pthread_t current_thread = pthread_self();
    int result = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
    
    if (result != 0) {
        std::cerr << "[WARNING] Linux pthread_setaffinity_np failed for core " << core_id << std::endl;
        return false;
    }
    return true;

#elif defined(_WIN32)
    // Windows expects a bitmask. For Core N, we shift 1 by N.
    // E.g., Core 0 = 0b0001, Core 1 = 0b0010
    DWORD_PTR mask = (static_cast<DWORD_PTR>(1) << core_id);
    HANDLE current_thread = GetCurrentThread();
    
    DWORD_PTR result = SetThreadAffinityMask(current_thread, mask);
    
    if (result == 0) {
        std::cerr << "[WARNING] Windows SetThreadAffinityMask failed for core " << core_id 
                  << ". Error code: " << GetLastError() << std::endl;
        return false;
    }
    return true;

#else
    std::cerr << "[WARNING] Thread pinning is not supported on this architecture." << std::endl;
    return false;
#endif
}

} // namespace concurrency
} // namespace matching_engine
