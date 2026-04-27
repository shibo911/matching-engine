#pragma once

#include <cstdint>
#include <string>

namespace matching_engine {
namespace gateway {

// ========================================================================
// Gateway Configuration
// ========================================================================

struct GatewayConfig {
    // Network
    uint16_t port = 8080;
    std::string host = "0.0.0.0";
    
    // Performance
    uint32_t broadcast_fps = 20;      // Frames per second
    uint32_t max_connections = 1000;  // Maximum concurrent connections
    uint32_t send_timeout_ms = 1000;  // Send timeout
    
    // Queue
    uint32_t queue_batch_size = 64;   // Messages per batch
    
    // Security (optional)
    bool enable_compression = false;  // Disabled for lowest latency
    uint32_t max_message_size = 65536; // 64KB
    
    // Validation
    bool validate() const {
        return port > 0 && port < 65536 &&
               broadcast_fps > 0 && broadcast_fps <= 1000 &&
               max_connections > 0 && max_connections <= 10000 &&
               queue_batch_size > 0 && queue_batch_size <= 1024 &&
               max_message_size > 0 && max_message_size <= 16 * 1024 * 1024; // 16MB max
    }
};

// ========================================================================
// Gateway Statistics
// ========================================================================

struct GatewayStats {
    uint64_t uptime_ms = 0;
    uint64_t connections_active = 0;
    uint64_t connections_total = 0;
    uint64_t messages_sent = 0;
    uint64_t bytes_sent = 0;
    uint64_t queue_depth = 0;
    double messages_per_second = 0.0;
    
    // Timestamps for rate calculation
    uint64_t last_update_ms = 0;
    uint64_t messages_sent_since_last_update = 0;
};

} // namespace gateway
} // namespace matching_engine