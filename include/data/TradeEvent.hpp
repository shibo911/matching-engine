#pragma once
#include <cstdint>
#include "Order.hpp"

namespace matching_engine {
namespace data {

// Represents an executed trade payload sent to the WebSockets.
// Size: 40 bytes (Fits perfectly into cache lines - 64-byte cache line).
// Layout: 3 uint64 (24) + 3 uint32 (12) + uint8 (1) + 3 padding = 40 bytes.
struct TradeEvent {
    uint64_t trade_id;
    uint64_t maker_order_id;
    uint64_t taker_order_id;
    uint32_t timestamp;        // milliseconds since epoch
    uint32_t price;           // in cents (15025 = $150.25)
    uint32_t quantity;        // shares
    Side side;                // Buy or Sell (uint8_t)
    uint8_t reserved[3];      // padding to align to 8 bytes
};

// ========================================================================
// MECHANICAL SYMPATHY: COMPILE-TIME PACKING ENFORCEMENT
// ========================================================================
// 3*8=24 + 3*4=12 + 1 + 3 = 40 bytes exactly.
// Fits perfectly in a 64-byte cache line with 24 bytes to spare.
static_assert(sizeof(TradeEvent) == 40, "TradeEvent must be exactly 40 bytes for cache alignment!");
