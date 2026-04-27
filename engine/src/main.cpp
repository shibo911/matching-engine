#include <iostream>
#include <vector>
#include <chrono>
#include <new>
#include <cstdlib>
#include <thread>
#include <random>
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
#include "gateway/WebSocketBroadcaster.hpp"
#include "gateway/WebSocketGateway.hpp"
#include "gateway/Config.hpp"

using namespace matching_engine;

// Simulated Network/Gateway message
struct IngressMessage {
    uint64_t order_id;
    uint32_t price;
    uint32_t quantity;
    bool is_cancel;
    matching_engine::data::Side side;
};
static_assert(sizeof(IngressMessage) == 24, "IngressMessage packing compromised!");

int main() {
    using namespace matching_engine::memory;
    using namespace matching_engine::data;
    using namespace matching_engine::concurrency;
    using namespace matching_engine::core;
    using namespace matching_engine::gateway;

    try {
        std::cout << "[INIT] Bootstrapping Engine Memory (700MB)..." << std::endl;
        EngineMemory memory(734003200); 
        std::pmr::polymorphic_allocator<std::byte> alloc = memory.get_allocator();

        const uint32_t MIN_PRICE = 10000;
        const uint32_t MAX_PRICE = 30000;
        const size_t MAX_ORDERS = 10000000; 

        LimitOrderBook lob(MIN_PRICE, MAX_PRICE, alloc);
        CancelLookup cancel_lookup(MAX_ORDERS, alloc);
        ObjectPool<Order> order_pool(MAX_ORDERS, alloc);
        
        SPSCQueue<IngressMessage, 1024> ingress_queue(alloc);
        SPMCBroadcastQueue<TradeEvent, 1024> egress_queue(alloc);

        MatchingEngine engine(&lob, &cancel_lookup, &order_pool, &egress_queue);

        // Configuration for WebSocket Gateway
        gateway::GatewayConfig ws_config;
        ws_config.port = 8080;
        ws_config.broadcast_fps = 20;
        ws_config.max_connections = 1000;
        
        std::cout << "[INIT] Starting uWebSockets Gateway on ws://127.0.0.1:8080..." << std::endl;
        std::cout << "[INIT] Health check: http://127.0.0.1:8080/health" << std::endl;
        std::cout << "[INIT] Metrics: http://127.0.0.1:8080/metrics" << std::endl;
        
        gateway::WebSocketGateway ws_gateway(egress_queue, ws_config);
        if (!ws_gateway.start()) {
            std::cerr << "[ERROR] Failed to start WebSocket Gateway" << std::endl;
            return 1;
        }
        
        std::cout << "[SUCCESS] Component initialization complete.\n" << std::endl;

        std::atomic<bool> running{true};

        // --- THE PINNED MATCHING ENGINE THREAD ---
        std::thread engine_thread([&]() {
            pin_current_thread_to_core(1);
            IngressMessage msg;
            while (running) {
                if (ingress_queue.pop(msg)) {
                    if (msg.is_cancel) {
                        Order* to_cancel = cancel_lookup.get_order(msg.order_id); 
                        if (to_cancel) {
                            lob.remove_order(to_cancel);               
                            cancel_lookup.deregister_order(msg.order_id);
                            order_pool.deallocate(to_cancel);
                        }
                    } else {
                        Order* order = order_pool.allocate(nullptr, nullptr, msg.order_id, msg.price, msg.quantity, msg.side);
                        engine.process_order(order);
                    }
                } else {
                    // Small yield to prevent 100% CPU burn when idle in this simulation
                    std::this_thread::yield();
                }
            }
        });

        // --- THE NETWORK SIMULATOR THREAD ---
        // Generates continuous random market data to feed the UI
        std::thread network_thread([&]() {
            std::mt19937 rng(1337);
            std::uniform_int_distribution<uint32_t> price_dist(14500, 15500);
            std::uniform_int_distribution<uint32_t> qty_dist(1, 100);
            std::uniform_int_distribution<int> side_dist(0, 1);
            
            uint64_t order_id = 1;
            while (running) {
                IngressMessage msg;
                msg.order_id = order_id++;
                msg.price = price_dist(rng);
                msg.quantity = qty_dist(rng);
                msg.is_cancel = false;
                msg.side = side_dist(rng) == 0 ? Side::Buy : Side::Sell;
                
                while (!ingress_queue.push(msg)) {} // Spin until space
                
                // Fire ~100 orders per second to simulate active market without freezing UI
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

        std::cout << ">>> Engine is running and broadcasting. Press ENTER to stop. <<<" << std::endl;
        std::cin.get();
        
        running = false;
        engine_thread.join();
        network_thread.join();
        ws_gateway.stop();

    } catch (const std::exception& e) {
        std::cerr << "[FATAL] Exception: " << e.what() << std::endl;
    }

    return 0;
}
