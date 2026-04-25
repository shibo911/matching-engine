#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>
#include "data/PriceLevel.hpp"

namespace matching_engine {
namespace data {

class LimitOrderBook {
public:
    LimitOrderBook(uint32_t min_price, uint32_t max_price, std::pmr::polymorphic_allocator<std::byte> alloc)
        : min_price_(min_price), 
          max_price_(max_price),
          price_levels_(alloc)
    {
        if (max_price < min_price) {
            throw std::invalid_argument("max_price must be >= min_price");
        }
        
        // Pre-allocate the entire price continuum. Zero heap allocations during trading.
        price_levels_.resize(max_price - min_price + 1);
    }
    
    LimitOrderBook(const LimitOrderBook&) = delete;
    LimitOrderBook& operator=(const LimitOrderBook&) = delete;
    LimitOrderBook(LimitOrderBook&&) = delete;
    LimitOrderBook& operator=(LimitOrderBook&&) = delete;

    // Maps a raw integer price directly to its memory index in O(1).
    inline PriceLevel* get_price_level(uint32_t price) {
        // Bounds checking is technically optional if the Gateway guarantees safe inputs.
        // We use [[unlikely]] to hint the branch predictor to optimize for valid bounds.
        if (price < min_price_ || price > max_price_) [[unlikely]] {
            return nullptr; 
        }
        return &price_levels_[price - min_price_];
    }

    inline void add_order(Order* order) {
        PriceLevel* level = get_price_level(order->price);
        if (level) [[likely]] {
            level->orders.push_back(order);
            level->total_volume += order->quantity;
        }
    }

    inline void remove_order(Order* order) {
        PriceLevel* level = get_price_level(order->price);
        if (level) [[likely]] {
            level->orders.remove(order);
            level->total_volume -= order->quantity;
        }
    }

    inline uint32_t min_price() const { return min_price_; }
    inline uint32_t max_price() const { return max_price_; }

private:
    uint32_t min_price_;
    uint32_t max_price_;
    
    // A single contiguous array handling both bids and asks.
    // Iterating through prices is perfectly cache-friendly due to linear memory layout.
    std::pmr::vector<PriceLevel> price_levels_;
};

} // namespace data
} // namespace matching_engine
