#pragma once

#include <atomic>
#include <vector>
#include <cstddef>
#include <cstdint>

namespace matching_engine {
namespace gateway {

// ========================================================================
// Lock-Free Connection Set for WebSocket Connections
// ========================================================================
// Thread-safe connection management using atomic operations
// Supports O(1) add/remove and efficient broadcast to all connections
// ========================================================================

class LockFreeConnectionSet {
public:
    // Connection handle - stores uWebSockets connection pointer
    struct ConnectionHandle {
        void* ws;      // uWebSockets connection pointer (uWS::WebSocket<false, true>*)
        uint64_t id;   // Unique identifier for the connection
        
        ConnectionHandle() : ws(nullptr), id(0) {}
        ConnectionHandle(void* ws_ptr, uint64_t connection_id) 
            : ws(ws_ptr), id(connection_id) {}
        
        bool is_valid() const { return ws != nullptr; }
    };
    
    // Constructor with configurable capacity
    explicit LockFreeConnectionSet(size_t capacity = 1024);
    ~LockFreeConnectionSet();
    
    // Non-copyable, non-movable
    LockFreeConnectionSet(const LockFreeConnectionSet&) = delete;
    LockFreeConnectionSet& operator=(const LockFreeConnectionSet&) = delete;
    LockFreeConnectionSet(LockFreeConnectionSet&&) = delete;
    LockFreeConnectionSet& operator=(LockFreeConnectionSet&&) = delete;
    
    // Thread-safe operations
    bool add(void* ws_connection);
    bool remove(void* ws_connection);
    
    // Broadcast data to all active connections
    // Returns number of connections successfully broadcasted to
    size_t broadcast(const void* data, size_t length);
    
    // Get current number of active connections
    size_t size() const;
    
    // Get capacity
    size_t capacity() const { return slots_.size(); }
    
    // Clear all connections (not thread-safe, use with caution)
    void clear();
    
private:
    // Internal slot structure with versioning for safe iteration
    struct Slot {
        std::atomic<void*> connection{nullptr};
        std::atomic<uint64_t> version{0};
        
        // Load the slot atomically
        ConnectionHandle load() const {
            return ConnectionHandle(connection.load(std::memory_order_acquire),
                                   version.load(std::memory_order_acquire));
        }
        
        // Store to the slot atomically
        void store(void* conn, uint64_t ver) {
            connection.store(conn, std::memory_order_release);
            version.store(ver, std::memory_order_release);
        }
        
        // Compare and swap for lock-free updates
        bool compare_exchange(void*& expected_conn, uint64_t& expected_ver,
                             void* new_conn, uint64_t new_ver) {
            bool conn_success = connection.compare_exchange_strong(
                expected_conn, new_conn, std::memory_order_acq_rel);
            bool ver_success = version.compare_exchange_strong(
                expected_ver, new_ver, std::memory_order_acq_rel);
            return conn_success && ver_success;
        }
    };
    
    std::vector<Slot> slots_;
    std::atomic<size_t> count_{0};
    std::atomic<uint64_t> next_id_{1};  // For unique connection IDs
    
    static constexpr size_t MAX_RETRIES = 3;
    
    // Helper functions
    size_t find_slot(void* ws_connection) const;
    size_t find_empty_slot() const;
};

} // namespace gateway
} // namespace matching_engine