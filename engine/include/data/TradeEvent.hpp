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

} // namespace data
} // namespace matching_engine
