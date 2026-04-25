#pragma once
#include <cstdint>

namespace matching_engine {
namespace data {

// Represents an executed trade payload sent to the WebSockets.
// Size: 32 bytes (Fits gracefully into cache lines).
struct TradeEvent {
    uint64_t trade_id;
    uint64_t maker_order_id;
    uint64_t taker_order_id;
    uint32_t price;
    uint32_t quantity;
};

// ========================================================================
// MECHANICAL SYMPATHY: COMPILE-TIME PACKING ENFORCEMENT
// ========================================================================
// 3 uint64 (24) + 2 uint32 (8) = 32 bytes exactly. Zero padding.
// Perfectly fills exactly half of an x86 64-byte Cache Line.
static_assert(sizeof(TradeEvent) == 32, "TradeEvent must be exactly 32 bytes for optimal broadcast performance!");

} // namespace data
} // namespace matching_engine
