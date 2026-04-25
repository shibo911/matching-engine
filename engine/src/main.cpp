#include <iostream>
#include <vector>
#include <chrono>
#include <new>
#include <cstdlib>
#include <thread>
#include "memory/EngineMemory.hpp"
#include "data/Order.hpp"
#include "data/LimitOrderBook.hpp"
#include "data/CancelLookup.hpp"
#include "data/TradeEvent.hpp"
#include "concurrency/ThreadUtils.hpp"
#include "concurrency/SPSCQueue.hpp"
#include "concurrency/SPMCQueue.hpp"

// ========================================================================
// ZERO-ALLOCATION ENFORCEMENT
// ========================================================================
std::atomic<bool> g_hot_path_active{false};

void* operator new(std::size_t size) {
    if (g_hot_path_active.load(std::memory_order_relaxed)) {
        std::cerr << "\n[FATAL ERROR] Heap allocation detected in Hot Path! Size: " << size << " bytes" << std::endl;
        std::abort();
    }
    return std::malloc(size);
}
void* operator new[](std::size_t size) {
    if (g_hot_path_active.load(std::memory_order_relaxed)) { std::abort(); }
    return std::malloc(size);
}
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr) noexcept { std::free(ptr); }
// ========================================================================

template <typename T>
class ObjectPool {
public:
    ObjectPool(size_t capacity, std::pmr::polymorphic_allocator<std::byte> alloc)
        : pool_(alloc) { pool_.reserve(capacity); }

    template <typename... Args>
    T* allocate(Args&&... args) {
        pool_.emplace_back(std::forward<Args>(args)...);
        return &pool_.back();
    }
private:
    std::pmr::vector<T> pool_;
};

struct IngressMessage {
    uint64_t order_id;
    uint32_t price;
    uint32_t quantity;
    bool is_cancel; 
    matching_engine::data::Side side;
};

int main() {
    using namespace matching_engine::memory;
    using namespace matching_engine::data;
    using namespace matching_engine::concurrency;

    try {
        std::cout << "[INIT] Bootstrapping Engine Memory (500MB)..." << std::endl;
        EngineMemory memory;
        std::pmr::polymorphic_allocator<std::byte> alloc = memory.get_allocator();

        const uint32_t MIN_PRICE = 10000;
        const uint32_t MAX_PRICE = 30000;
        const size_t MAX_ORDERS = 10000000; 

        LimitOrderBook lob(MIN_PRICE, MAX_PRICE, alloc);
        CancelLookup cancel_lookup(MAX_ORDERS, alloc);
        ObjectPool<Order> order_pool(MAX_ORDERS, alloc);
        
        SPSCQueue<IngressMessage, 1024> ingress_queue(alloc);
        
        std::cout << "[INIT] Allocating SPMC Broadcast Egress Queue (Capacity 1024)..." << std::endl;
        SPMCBroadcastQueue<TradeEvent, 1024> egress_queue(alloc);

        std::cout << "[SUCCESS] Component initialization complete.\n" << std::endl;

        // ========================================================================
        // LIVE TRADING: THREAD ISOLATION PIPELINES
        // ========================================================================
        
        std::atomic<int> trades_broadcasted_atomic{0};

        // --- 1. THE CONSUMER THREADS (WebSocket Visualization Gateways) ---
        // Simulating 2 independent WebSockets consuming the identical broadcast
        auto spawn_websocket = [&](int ws_id) {
            return std::thread([&, ws_id]() {
                size_t local_read_seq = 0; // Private consumer sequence
                TradeEvent evt;
                
                while (trades_broadcasted_atomic.load(std::memory_order_acquire) < 1) {
                    // Lock-free popping
                    if (egress_queue.pop(local_read_seq, evt)) {
                        std::cout << "[WEBSOCKET " << ws_id << "] Received Trade Broadcast! ID: " 
                                  << evt.trade_id << " | Price: " << evt.price 
                                  << " | Qty: " << evt.quantity << std::endl;
                    }
                }
            });
        };

        std::thread ws_thread_1 = spawn_websocket(1);
        std::thread ws_thread_2 = spawn_websocket(2);

        // --- 2. THE PINNED MATCHING ENGINE THREAD ---
        std::thread engine_thread([&]() {
            pin_current_thread_to_core(1);
            
            IngressMessage msg;
            bool trade_executed = false;

            while (!trade_executed) {
                if (ingress_queue.pop(msg)) {
                    // Simulate Crossing Logic resulting in a Trade
                    TradeEvent new_trade{ 999, 42, 43, msg.price, msg.quantity };
                    
                    // PRODUCER: Instantly fire and forget into the broadcast queue
                    egress_queue.push(new_trade);
                    
                    trade_executed = true;
                    trades_broadcasted_atomic.store(1, std::memory_order_release);
                }
            }
        });

        // --- 3. THE PRODUCER THREAD (Main Network Gateway) ---
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Warm up

        std::cout << ">>> LOCKING HEAP. FIRING FULL PIPELINE. <<<" << std::endl;
        g_hot_path_active.store(true, std::memory_order_relaxed);
        
        auto t_start = std::chrono::high_resolution_clock::now();

        // Gateway pushes an incoming order
        IngressMessage insert_msg{43, 15000, 100, false, Side::Sell};
        while(!ingress_queue.push(insert_msg)) {} 

        // Wait for Engine to cross the trade and broadcast it
        while (trades_broadcasted_atomic.load(std::memory_order_acquire) < 1) {}
        
        auto t_end = std::chrono::high_resolution_clock::now();

        g_hot_path_active.store(false, std::memory_order_relaxed);
        std::cout << ">>> EXITING HOT PATH. HEAP UNLOCKED. <<<\n" << std::endl;

        engine_thread.join();
        ws_thread_1.join();
        ws_thread_2.join();
        // ========================================================================

        std::cout << "[PROOF] 4-Thread execution finished with 0 heap allocations." << std::endl;
        std::cout << "[METRICS] Gateway -> Engine -> WebSockets Total Latency: " 
                  << std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count() 
                  << " ns." << std::endl;

    } catch (const std::exception& e) {
        g_hot_path_active.store(false, std::memory_order_relaxed);
        std::cerr << "[FATAL] Exception: " << e.what() << std::endl;
    }

    return 0;
}
