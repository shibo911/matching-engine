#pragma once

#include <cstdint>
#include <cstring>
#include "data/TradeEvent.hpp"

namespace matching_engine {
namespace gateway {

// ========================================================================
// Binary Protocol for WebSocket Transmission
// ========================================================================
// 32-byte packed binary format for TradeEvent transmission
// No padding, no overhead, optimal for network transmission
// ========================================================================

#pragma pack(push, 1)  // No padding for binary transmission
struct BinaryTradeEvent {
    uint64_t trade_id;
    uint64_t maker_order_id;
    uint64_t taker_order_id;
    uint32_t timestamp;      // milliseconds since epoch
    uint32_t price;          // in cents (15025 = $150.25)
    uint32_t quantity;       // shares
    uint8_t side;            // 0 = Buy, 1 = Sell (matches Side enum)
    uint8_t reserved[3];     // padding to align to 8 bytes
    
    // Validation - must match exactly the size of data::TradeEvent
    static_assert(sizeof(BinaryTradeEvent) == 40,
                  "BinaryTradeEvent must be exactly 40 bytes for cache alignment");
};
#pragma pack(pop)

// ========================================================================
// Serialization Utilities
// ========================================================================

// Convert TradeEvent to BinaryTradeEvent (memory copy with validation)
inline void trade_event_to_binary(const data::TradeEvent& src, BinaryTradeEvent& dst) {
    // Direct memory copy since structures have identical layout
    // This is safe because both are 40 bytes with same field order
    static_assert(sizeof(data::TradeEvent) == sizeof(BinaryTradeEvent),
                  "TradeEvent and BinaryTradeEvent must have same size");
    
    // Use memcpy for type safety (avoids strict aliasing violations)
    std::memcpy(&dst, &src, sizeof(BinaryTradeEvent));
}

// Convert BinaryTradeEvent to TradeEvent
inline void binary_to_trade_event(const BinaryTradeEvent& src, data::TradeEvent& dst) {
    static_assert(sizeof(data::TradeEvent) == sizeof(BinaryTradeEvent),
                  "TradeEvent and BinaryTradeEvent must have same size");
    std::memcpy(&dst, &src, sizeof(data::TradeEvent));
}

// Create BinaryTradeEvent from TradeEvent (return by value)
inline BinaryTradeEvent to_binary(const data::TradeEvent& src) {
    BinaryTradeEvent dst;
    trade_event_to_binary(src, dst);
    return dst;
}

// Create TradeEvent from BinaryTradeEvent (return by value)
inline data::TradeEvent from_binary(const BinaryTradeEvent& src) {
    data::TradeEvent dst;
    binary_to_trade_event(src, dst);
    return dst;
}

// ========================================================================
// Endianness Utilities (for cross-platform compatibility)
// ========================================================================

// Check if system is little-endian (most modern systems are)
constexpr bool is_little_endian() {
    constexpr uint32_t test = 0x01020304;
    return reinterpret_cast<const uint8_t*>(&test)[0] == 0x04;
}

// Convert to network byte order (big-endian) if needed
// For now, we assume little-endian systems and clients
// In production, you'd want to standardize on network byte order
inline void to_network_byte_order(BinaryTradeEvent& event) {
    if (!is_little_endian()) {
        // If system is big-endian, convert to little-endian for transmission
        // This is a placeholder - actual implementation would swap bytes
        // For now, we'll trust that all systems are little-endian
    }
}

// Convert from network byte order to host byte order
inline void from_network_byte_order(BinaryTradeEvent& event) {
    if (!is_little_endian()) {
        // If system is big-endian, convert from little-endian
        // This is a placeholder
    }
}

} // namespace gateway
} // namespace matching_engine