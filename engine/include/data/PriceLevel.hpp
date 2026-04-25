#pragma once
#include <cstdint>
#include "data/IntrusiveOrderList.hpp"

namespace matching_engine {
namespace data {

// Represents a single tick price level in the flat LOB array.
// Size: 24 bytes. Two adjacent PriceLevels fit cleanly inside one 64-byte cache line.
struct alignas(8) PriceLevel {
    IntrusiveOrderList orders; // 16 bytes 
    uint64_t total_volume = 0; // 8 bytes

    // Note: We don't store Side (Bid/Ask) here because bids and asks can never 
    // simultaneously rest at the same price. By omitting it, we save padding space.
};

} // namespace data
} // namespace matching_engine
