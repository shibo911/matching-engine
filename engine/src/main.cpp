#include <iostream>
#include <vector>
#include <chrono>
#include <new>
#include <cstdlib>
#include <thread>
#include "memory/EngineMemory.hpp"
#include "memory/ObjectPool.hpp"
#include "data/Order.hpp"
#include "data/LimitOrderBook.hpp"
#include "data/CancelLookup.hpp"
#include "data/TradeEvent.hpp"
#include "concurrency/ThreadUtils.hpp"
#include "concurrency/SPSCQueue.hpp"
#include "concurrency/SPMCQueue.hpp"
#include "core/MatchingEngine.hpp"

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

struct IngressMessage {
    uint64_t order_id;  // 8 bytes
    uint32_t price;     // 4 bytes
    uint32_t quantity;  // 4 bytes
    bool is_cancel;     // 1 byte
    matching_engine::data::Side side; // 1 byte
};

// 8 + 4 + 4 + 1 + 1 = 18 bytes. Padded to 24.
static_assert(sizeof(IngressMessage) == 24, "IngressMessage packing compromised!");

int main() {
    using namespace matching_engine::memory;
    using namespace matching_engine::data;
    using namespace matching_engine::concurrency;
    using namespace matching_engine::core;

    try {
        std::cout << "[INIT] Bootstrapping Engine Memory (700MB)..." << std::endl;
        EngineMemory memory(734003200); // 700 MB to accommodate the new Free List arrays
        std::pmr::polymorphic_allocator<std::byte> alloc = memory.get_allocator();

        const uint32_t MIN_PRICE = 10000;
        const uint32_t MAX_PRICE = 30000;
        const size_t MAX_ORDERS = 10000000; 

        LimitOrderBook lob(MIN_PRICE, MAX_PRICE, alloc);
        CancelLookup cancel_lookup(MAX_ORDERS, alloc);
        ObjectPool<Order> order_pool(MAX_ORDERS, alloc);
        
        SPSCQueue<IngressMessage, 1024> ingress_queue(alloc);
        SPMCBroadcastQueue<TradeEvent, 1024> egress_queue(alloc);

        // Core matching logic
        MatchingEngine engine(&lob, &cancel_lookup, &order_pool, &egress_queue);

        std::cout << "[SUCCESS] Component initialization complete.\n" << std::endl;

        // ========================================================================
        // LIVE TRADING: THREAD ISOLATION PIPELINES
        // ========================================================================
        
        std::atomic<int> trades_broadcasted_atomic{0};

        // --- 1. THE CONSUMER THREAD (WebSocket Gateway) ---
        std::thread ws_thread([&]() {
            size_t local_read_seq = 0; 
            TradeEvent evt;
            
            while (trades_broadcasted_atomic.load(std::memory_order_acquire) < 1) {
                if (egress_queue.pop(local_read_seq, evt)) {
                    std::cout << "[WEBSOCKET] Match Executed! TradeID: " << evt.trade_id 
                              << " | Maker: " << evt.maker_order_id << " | Taker: " << evt.taker_order_id
                              << " | Price: " << evt.price << " | Qty: " << evt.quantity << std::endl;
                    trades_broadcasted_atomic.store(1, std::memory_order_release);
                }
            }
        });

        // --- 2. THE PINNED MATCHING ENGINE THREAD ---
        std::thread engine_thread([&]() {
            pin_current_thread_to_core(1);
            
            IngressMessage msg;
            int messages_processed = 0;

            // We expect exactly 2 messages from the Gateway
            while (messages_processed < 2) {
                if (ingress_queue.pop(msg)) {
                    if (msg.is_cancel) {
                        Order* to_cancel = cancel_lookup.get_order(msg.order_id); 
                        if (to_cancel) {
                            lob.remove_order(to_cancel);               
                            cancel_lookup.deregister_order(msg.order_id);
                            order_pool.deallocate(to_cancel);
                        }
                    } else {
                        // Dynamically allocate from Object Pool
                        Order* order = order_pool.allocate(nullptr, nullptr, msg.order_id, msg.price, msg.quantity, msg.side);
                        
                        // Pass to the ultra-low-latency crossing algorithm!
                        engine.process_order(order);
                    }
                    messages_processed++;
                }
            }
        });

        // --- 3. THE PRODUCER THREAD (Main Network Gateway) ---
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Warm up

        std::cout << ">>> LOCKING HEAP. FIRING FULL PIPELINE. <<<" << std::endl;
        g_hot_path_active.store(true, std::memory_order_relaxed);
        
        auto t_start = std::chrono::high_resolution_clock::now();

        // Gateway pushes a Sell order (Making liquidity)
        IngressMessage make_msg{101, 15000, 100, false, Side::Sell};
        while(!ingress_queue.push(make_msg)) {} 

        // Gateway pushes a Buy order (Crossing the book! Takes liquidity)
        IngressMessage cross_msg{102, 15000, 50, false, Side::Buy};
        while(!ingress_queue.push(cross_msg)) {} 

        // Wait for Engine to cross the trade and broadcast it
        while (trades_broadcasted_atomic.load(std::memory_order_acquire) < 1) {}
        
        auto t_end = std::chrono::high_resolution_clock::now();

        g_hot_path_active.store(false, std::memory_order_relaxed);
        std::cout << ">>> EXITING HOT PATH. HEAP UNLOCKED. <<<\n" << std::endl;

        engine_thread.join();
        ws_thread.join();
        // ========================================================================

        std::cout << "[PROOF] Full algorithmic crossing executed with 0 heap allocations." << std::endl;
        std::cout << "[METRICS] Gateway -> Crossing Engine -> WebSocket Total Latency: " 
                  << std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count() 
                  << " ns." << std::endl;

    } catch (const std::exception& e) {
        g_hot_path_active.store(false, std::memory_order_relaxed);
        std::cerr << "[FATAL] Exception: " << e.what() << std::endl;
    }

    return 0;
}
