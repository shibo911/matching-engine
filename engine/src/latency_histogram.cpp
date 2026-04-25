#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>
#include <thread>
#include "core/MatchingEngine.hpp"
#include "memory/EngineMemory.hpp"
#include "data/Order.hpp"
#include "concurrency/ThreadUtils.hpp"
#include "concurrency/SPSCQueue.hpp"
#include "concurrency/SPMCQueue.hpp"

using namespace matching_engine;

// Copied from main.cpp to isolate benchmarking
struct IngressMessage {
    uint64_t order_id;
    uint32_t price;
    uint32_t quantity;
    bool is_cancel;
    matching_engine::data::Side side;
};
static_assert(sizeof(IngressMessage) == 24, "IngressMessage packing compromised!");

int main() {
    using namespace std::chrono;
    
    // 500MB is massive, easily fits our 1M orders + internal memory
    memory::EngineMemory mem(1024 * 1024 * 500); 
    auto alloc = mem.get_allocator();
    data::LimitOrderBook lob(10000, 30000, alloc);
    
    // Simulate 1 Million events 
    size_t num_events = 1000000; 
    data::CancelLookup cancel(num_events * 2, alloc);
    memory::ObjectPool<data::Order> pool(num_events * 2, alloc);
    concurrency::SPMCBroadcastQueue<data::TradeEvent, 1024> egress(alloc);
    concurrency::SPSCQueue<IngressMessage, 1024> ingress(alloc);
    
    core::MatchingEngine engine(&lob, &cancel, &pool, &egress);

    // Pre-allocate the vector so we NEVER trigger OS heap expansion during the hot path
    std::vector<uint64_t> latencies;
    latencies.reserve(num_events);

    // Gateway (Network) Thread
    std::thread gateway([&]() {
        for (size_t i = 0; i < num_events; ++i) {
            IngressMessage msg;
            msg.order_id = i + 1;
            msg.price = 15000 + (i % 10); // Small spread
            msg.quantity = 10;
            msg.is_cancel = false;
            
            // Alternate Buy and Sell to violently trigger crosses continuously
            msg.side = (i % 2 == 0) ? data::Side::Sell : data::Side::Buy;
            
            while (!ingress.push(msg)) {
                // Wait-Free Spin
            }
        }
    });

    // Pinned Engine Thread
    std::thread engine_thread([&]() {
        concurrency::pin_current_thread_to_core(1);
        IngressMessage msg;
        size_t processed = 0;
        
        while (processed < num_events) {
            if (ingress.pop(msg)) {
                
                // ====================================================================
                // START MEASUREMENT: Instantly after SPSC pop
                // ====================================================================
                auto t1 = high_resolution_clock::now();
                
                data::Order* order = pool.allocate();
                order->id = msg.order_id;
                order->price = msg.price;
                order->quantity = msg.quantity;
                order->side = msg.side;
                
                // The actual Algorithm: Crosses order and pushes to SPMC Egress Wait-Free
                engine.process_order(order);
                
                // ====================================================================
                // END MEASUREMENT: Instantly after matching engine returns
                // ====================================================================
                auto t2 = high_resolution_clock::now();
                
                latencies.push_back(duration_cast<nanoseconds>(t2 - t1).count());
                processed++;
            }
        }
    });

    gateway.join();
    engine_thread.join();

    std::sort(latencies.begin(), latencies.end());
    
    std::cout << "\n=============================================" << std::endl;
    std::cout << "  LATENCY HISTOGRAM (Pop -> Cross -> Push)  " << std::endl;
    std::cout << "=============================================" << std::endl;
    std::cout << " Total Events : " << num_events << " (1 Million)" << std::endl;
    std::cout << "---------------------------------------------" << std::endl;
    std::cout << " p50 (Median) : " << latencies[num_events * 0.50] << " ns" << std::endl;
    std::cout << " p90          : " << latencies[num_events * 0.90] << " ns" << std::endl;
    std::cout << " p99          : " << latencies[num_events * 0.99] << " ns" << std::endl;
    std::cout << " p99.9        : " << latencies[num_events * 0.999] << " ns" << std::endl;
    std::cout << " p99.99       : " << latencies[num_events * 0.9999] << " ns" << std::endl;
    std::cout << " Max Jitter   : " << latencies.back() << " ns" << std::endl;
    std::cout << "=============================================\n" << std::endl;

    return 0;
}
