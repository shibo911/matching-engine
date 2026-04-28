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

// ========================================================================
// MECHANICAL SYMPATHY: COMPILE-TIME PACKING ENFORCEMENT
// ========================================================================
// List(16) + Volume(8) = 24 bytes. 
static_assert(sizeof(PriceLevel) == 24, "PriceLevel must be exactly 24 bytes so multiple levels fit into one L1 Cache Line!");

} // namespace data
} // namespace matching_engine
