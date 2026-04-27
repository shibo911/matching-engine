#include "gateway/ConnectionManager.hpp"
#include <algorithm>
#include <thread>

namespace matching_engine {
namespace gateway {

LockFreeConnectionSet::LockFreeConnectionSet(size_t capacity) 
    : slots_(capacity) {
    // Initialize all slots as empty
    for (size_t i = 0; i < capacity; ++i) {
        slots_[i].store(nullptr, 0);
    }
}

LockFreeConnectionSet::~LockFreeConnectionSet() {
    clear();
}

bool LockFreeConnectionSet::add(void* ws_connection) {
    if (ws_connection == nullptr) {
        return false;
    }
    
    // Check if already exists
    if (find_slot(ws_connection) != slots_.size()) {
        return false; // Already exists
    }
    
    // Try to find an empty slot
    for (size_t retry = 0; retry < MAX_RETRIES; ++retry) {
        size_t index = find_empty_slot();
        if (index == slots_.size()) {
            // No empty slots
            return false;
        }
        
        Slot& slot = slots_[index];
        void* expected_conn = nullptr;
        uint64_t expected_ver = slot.version.load(std::memory_order_acquire);
        
        // Generate new version (odd numbers indicate active connection)
        uint64_t new_ver = expected_ver + 1;
        uint64_t new_id = next_id_.fetch_add(1, std::memory_order_relaxed);
        
        // Try to claim the slot
        if (slot.compare_exchange(expected_conn, expected_ver, 
                                  ws_connection, new_ver)) {
            count_.fetch_add(1, std::memory_order_release);
            return true;
        }
        
        // CAS failed, retry with exponential backoff
        if (retry < MAX_RETRIES - 1) {
            std::this_thread::yield();
        }
    }
    
    return false;
}

bool LockFreeConnectionSet::remove(void* ws_connection) {
    if (ws_connection == nullptr) {
        return false;
    }
    
    size_t index = find_slot(ws_connection);
    if (index == slots_.size()) {
        return false; // Not found
    }
    
    Slot& slot = slots_[index];
    void* expected_conn = ws_connection;
    uint64_t expected_ver = slot.version.load(std::memory_order_acquire);
    
    // Invalidate the slot by setting connection to nullptr
    // and incrementing version (even numbers indicate empty slot)
    uint64_t new_ver = expected_ver + 1;
    
    if (slot.compare_exchange(expected_conn, expected_ver, 
                              nullptr, new_ver)) {
        count_.fetch_sub(1, std::memory_order_release);
        return true;
    }
    
    return false;
}

size_t LockFreeConnectionSet::broadcast(const void* data, size_t length) {
    if (data == nullptr || length == 0 || size() == 0) {
        return 0;
    }
    
    size_t broadcast_count = 0;
    
    // Iterate through all slots
    for (size_t i = 0; i < slots_.size(); ++i) {
        Slot& slot = slots_[i];
        ConnectionHandle handle = slot.load();
        
        // Check if slot is occupied (connection != nullptr and version is odd)
        if (handle.is_valid() && (handle.id % 2 == 1)) {
            // In a real implementation, we would send the data to the connection
            // For now, we just count it as successfully broadcasted
            // The actual sending will be implemented in WebSocketGateway
            ++broadcast_count;
        }
    }
    
    return broadcast_count;
}

size_t LockFreeConnectionSet::size() const {
    return count_.load(std::memory_order_acquire);
}

void LockFreeConnectionSet::clear() {
    // Not thread-safe - should only be used during shutdown
    for (auto& slot : slots_) {
        slot.store(nullptr, slot.version.load(std::memory_order_relaxed) + 1);
    }
    count_.store(0, std::memory_order_release);
}

size_t LockFreeConnectionSet::find_slot(void* ws_connection) const {
    if (ws_connection == nullptr) {
        return slots_.size();
    }
    
    for (size_t i = 0; i < slots_.size(); ++i) {
        const Slot& slot = slots_[i];
        ConnectionHandle handle = slot.load();
        
        if (handle.ws == ws_connection && (handle.id % 2 == 1)) {
            return i;
        }
    }
    
    return slots_.size(); // Not found
}

size_t LockFreeConnectionSet::find_empty_slot() const {
    for (size_t i = 0; i < slots_.size(); ++i) {
        const Slot& slot = slots_[i];
        ConnectionHandle handle = slot.load();
        
        // Empty slot: connection == nullptr OR version is even
        if (handle.ws == nullptr || (handle.id % 2 == 0)) {
            return i;
        }
    }
    
    return slots_.size(); // No empty slots
}

} // namespace gateway
} // namespace matching_engine