#pragma once
#include <cstdint>

namespace matching_engine {
namespace data {

enum class Side : uint8_t { Buy, Sell };

// Pack struct members from largest to smallest to avoid implicit compiler padding.
// The total size is 33 bytes, which the compiler aligns to 40 bytes.
// This guarantees that an Order struct will never straddle two 64-byte L1 cache lines.
struct Order {
    // 64-bit boundaries
    Order* next = nullptr;
    Order* prev = nullptr;
    uint64_t id = 0;

    // 32-bit boundaries
    uint32_t price = 0;
    uint32_t quantity = 0;

    // 8-bit boundaries
    Side side = Side::Buy;
};

// ========================================================================
// MECHANICAL SYMPATHY: COMPILE-TIME PACKING ENFORCEMENT
// ========================================================================
// Prevent future developers from adding variables that break L1 Cache alignment.
// 3 pointers (24) + 2 uint32 (8) + 1 uint8 (1) = 33 bytes.
// Padded to the nearest 8-byte boundary = 40 bytes.
static_assert(sizeof(Order) == 40, "Order struct packing has been compromised! Expected exactly 40 bytes.");
static_assert(alignof(Order) == 8, "Order struct must be aligned to 8-byte boundaries.");

} // namespace data
} // namespace matching_engine
