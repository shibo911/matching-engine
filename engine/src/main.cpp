#include <iostream>
#include <vector>
#include <chrono>
#include <new>
#include <cstdlib>
#include "memory/EngineMemory.hpp"
#include "data/Order.hpp"
#include "data/LimitOrderBook.hpp"
#include "data/CancelLookup.hpp"

// ========================================================================
// ZERO-ALLOCATION ENFORCEMENT
// Overriding global new/delete ensures we never silently allocate on the heap.
// If any STL container resizes during the hot path, the engine will abort.
// ========================================================================
bool g_hot_path_active = false;

void* operator new(std::size_t size) {
    if (g_hot_path_active) {
        std::cerr << "\n[FATAL ERROR] Heap allocation detected in Hot Path! Size: " << size << " bytes" << std::endl;
        std::abort();
    }
    return std::malloc(size);
}

void* operator new[](std::size_t size) {
    if (g_hot_path_active) {
        std::cerr << "\n[FATAL ERROR] Array Heap allocation detected in Hot Path! Size: " << size << " bytes" << std::endl;
        std::abort();
    }
    return std::malloc(size);
}

void operator delete(void* ptr) noexcept {
    if (g_hot_path_active && ptr != nullptr) {
        std::cerr << "\n[FATAL ERROR] Heap deallocation detected in Hot Path!" << std::endl;
        std::abort();
    }
    std::free(ptr);
}

void operator delete[](void* ptr) noexcept {
    if (g_hot_path_active && ptr != nullptr) {
        std::cerr << "\n[FATAL ERROR] Array Heap deallocation detected in Hot Path!" << std::endl;
        std::abort();
    }
    std::free(ptr);
}
// ========================================================================

// Minimal Order Pool simulation to bypass standard heap allocation for structs.
template <typename T>
class ObjectPool {
public:
    ObjectPool(size_t capacity, std::pmr::polymorphic_allocator<std::byte> alloc)
        : pool_(alloc) 
    {
        // For 10M orders * 40 bytes, this reserves exactly 400 MB.
        pool_.reserve(capacity); 
    }

    template <typename... Args>
    T* allocate(Args&&... args) {
        if (pool_.size() >= pool_.capacity()) [[unlikely]] {
            throw std::bad_alloc(); 
        }
        pool_.emplace_back(std::forward<Args>(args)...);
        return &pool_.back();
    }

private:
    std::pmr::vector<T> pool_;
};

int main() {
    using namespace matching_engine::memory;
    using namespace matching_engine::data;

    try {
        std::cout << "[INIT] Bootstrapping Engine Memory (500MB)..." << std::endl;
        EngineMemory memory;
        std::pmr::polymorphic_allocator<std::byte> alloc = memory.get_allocator();

        const uint32_t MIN_PRICE = 10000;
        const uint32_t MAX_PRICE = 30000;
        const size_t MAX_ORDERS = 10000000; 

        std::cout << "[INIT] Allocating Limit Order Book Array (~0.48 MB)..." << std::endl;
        LimitOrderBook lob(MIN_PRICE, MAX_PRICE, alloc);

        std::cout << "[INIT] Allocating Cancel Lookup Map (80 MB)..." << std::endl;
        CancelLookup cancel_lookup(MAX_ORDERS, alloc);

        std::cout << "[INIT] Allocating Order Object Pool (400 MB)..." << std::endl;
        ObjectPool<Order> order_pool(MAX_ORDERS, alloc);
        
        std::cout << "[SUCCESS] Total allocated: ~480.48 MB. Fits cleanly inside the 500MB monotonic buffer.\n" << std::endl;

        // ========================================================================
        // SIMULATING LIVE TRADING HOT PATH
        // ========================================================================
        std::cout << ">>> LOCKING HEAP. ENTERING HOT PATH. <<<" << std::endl;
        g_hot_path_active = true;
        
        auto t_start = std::chrono::high_resolution_clock::now();

        // 1. Gateway pushes an incoming order
        uint64_t order_id = 42;
        uint32_t price = 15000;
        Order* order = order_pool.allocate(nullptr, nullptr, order_id, price, 100, Side::Buy);
        
        // 2. Register for O(1) cancellations
        cancel_lookup.register_order(order);

        // 3. Place resting order in the LOB
        lob.add_order(order);

        // 4. Cancel workflow (usually triggered by client network request)
        Order* to_cancel = cancel_lookup.get_order(order_id); 
        if (to_cancel) {
            lob.remove_order(to_cancel);               
            cancel_lookup.deregister_order(order_id);  
        }

        auto t_end = std::chrono::high_resolution_clock::now();

        g_hot_path_active = false;
        std::cout << ">>> EXITING HOT PATH. HEAP UNLOCKED. <<<\n" << std::endl;
        // ========================================================================

        std::cout << "[PROOF] Execution finished with 0 heap allocations." << std::endl;
        std::cout << "[METRICS] End-to-end Insert + Cancel Latency: " 
                  << std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count() 
                  << " ns." << std::endl;

    } catch (const std::exception& e) {
        g_hot_path_active = false;
        std::cerr << "[FATAL] Exception: " << e.what() << std::endl;
    }

    return 0;
}
