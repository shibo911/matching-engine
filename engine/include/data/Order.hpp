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

} // namespace data
} // namespace matching_engine
