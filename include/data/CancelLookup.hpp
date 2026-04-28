#pragma once
#include <vector>
#include <cstdint>
#include "data/Order.hpp"

namespace matching_engine {
namespace data {

// Provides instantaneous order retrieval for fast cancellations.
class CancelLookup {
public:
    CancelLookup(size_t max_orders, std::pmr::polymorphic_allocator<std::byte> alloc)
        : map_(alloc)
    {
        // Pre-allocates the sparse array. For 10M orders, this takes exactly 80MB.
        map_.resize(max_orders, nullptr);
    }

    CancelLookup(const CancelLookup&) = delete;
    CancelLookup& operator=(const CancelLookup&) = delete;
    CancelLookup(CancelLookup&&) = delete;
    CancelLookup& operator=(CancelLookup&&) = delete;

    // Uses the internal OrderID as a direct memory index.
    // The Gateway thread must map any external string UUIDs to dense integers (0 to N).
    inline void register_order(Order* order) {
        if (order->id < map_.size()) [[likely]] {
            map_[order->id] = order;
        }
    }

    inline Order* get_order(uint64_t order_id) const {
        if (order_id < map_.size()) [[likely]] {
            return map_[order_id];
        }
        return nullptr;
    }

    inline void deregister_order(uint64_t order_id) {
        if (order_id < map_.size()) [[likely]] {
            map_[order_id] = nullptr;
        }
    }

    inline size_t capacity() const { return map_.size(); }

private:
    std::pmr::vector<Order*> map_;
};

} // namespace data
} // namespace matching_engine
