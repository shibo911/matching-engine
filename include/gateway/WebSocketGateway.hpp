#pragma once

#include <thread>
#include <atomic>
#include <memory>
#include <chrono>
#include <string>
#include "concurrency/SPMCQueue.hpp"
#include "data/TradeEvent.hpp"
#include "gateway/BinaryProtocol.hpp"
#include "gateway/ConnectionManager.hpp"
#include "gateway/Config.hpp"

// uWebSockets single-header include
#define ASIO_STANDALONE
#include <uWebSockets/App.h>

namespace matching_engine {
namespace gateway {

class WebSocketGateway {
public:
    WebSocketGateway(
        concurrency::SPMCBroadcastQueue<data::TradeEvent, 1024>& egress_queue,
        const GatewayConfig& config = GatewayConfig{}
    );
    
    ~WebSocketGateway();
    
    // Start the WebSocket server
    bool start();
    
    // Stop the server gracefully
    void stop();
    
    // Check if server is running
    bool is_running() const { return running_.load(std::memory_order_acquire); }
    
    // Get current statistics
    GatewayStats get_stats() const;
    
    // Get configuration
    const GatewayConfig& get_config() const { return config_; }
    
private:
    // uWebSockets event handlers
    void on_connection(uWS::WebSocket<false, true>* ws);
    void on_disconnection(uWS::WebSocket<false, true>* ws, int code, std::string_view message);
    void on_message(uWS::WebSocket<false, true>* ws, std::string_view message, uWS::OpCode opCode);
    
    // Background thread functions
    void broadcast_thread_func();
    void server_thread_func();
    
    // Statistics update
    void update_stats(uint64_t messages_sent, uint64_t bytes_sent);
    
    // Configuration
    GatewayConfig config_;
    
    // Queue reference
    concurrency::SPMCBroadcastQueue<data::TradeEvent, 1024>& egress_queue_;
    
    // Connection management
    LockFreeConnectionSet connections_;
    
    // Thread management
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    std::thread broadcast_thread_;
    
    // Statistics
    mutable std::atomic<uint64_t> messages_sent_{0};
    mutable std::atomic<uint64_t> connections_total_{0};
    mutable std::atomic<uint64_t> bytes_sent_{0};
    mutable std::atomic<uint64_t> start_time_ms_{0};
    
    // uWebSockets app
    std::unique_ptr<uWS::App> app_;
    
    // Health check and metrics endpoints flag
    bool enable_http_endpoints_ = true;
};

} // namespace gateway
} // namespace matching_engine