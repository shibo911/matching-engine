#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include "data/PriceLevel.hpp"

namespace matching_engine {
namespace data {

class LimitOrderBook {
public:
    LimitOrderBook(uint32_t min_price, uint32_t max_price, std::pmr::polymorphic_allocator<std::byte> alloc)
        : min_price_(min_price), 
          max_price_(max_price),
          price_levels_(alloc),
          best_bid_(0),
          best_ask_(max_price + 1)
    {
        if (max_price < min_price) {
            throw std::invalid_argument("max_price must be >= min_price");
        }
        
        // Pre-allocate the entire price continuum. Zero heap allocations during trading.
        price_levels_.resize(max_price - min_price + 1);
    }
    
    LimitOrderBook(const LimitOrderBook&) = delete;
    LimitOrderBook& operator=(const LimitOrderBook&) = delete;

    // Maps a raw integer price directly to its memory index in O(1).
    inline PriceLevel* get_price_level(uint32_t price) {
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

            // Maintain O(1) Best Bid and Best Ask index pointers
            if (order->side == Side::Buy) {
                if (order->price > best_bid_) best_bid_ = order->price;
            } else {
                if (order->price < best_ask_) best_ask_ = order->price;
            }
        }
    }

    inline void remove_order(Order* order) {
        PriceLevel* level = get_price_level(order->price);
        if (level) [[likely]] {
            level->orders.remove(order);
            level->total_volume -= order->quantity;

            // Recalculate BBO (Best Bid/Offer) if this level was emptied.
            // This 'while' loop executes extremely fast due to array cache locality.
            if (level->total_volume == 0) {
                if (order->side == Side::Buy && order->price == best_bid_) {
                    retreat_best_bid();
                } else if (order->side == Side::Sell && order->price == best_ask_) {
                    advance_best_ask();
                }
            }
        }
    }

    inline uint32_t min_price() const { return min_price_; }
    inline uint32_t max_price() const { return max_price_; }
    inline uint32_t best_bid() const { return best_bid_; }
    inline uint32_t best_ask() const { return best_ask_; }

    inline void advance_best_ask() {
        while (best_ask_ <= max_price_ && get_price_level(best_ask_)->total_volume == 0) {
            best_ask_++;
        }
    }

    inline void retreat_best_bid() {
        while (best_bid_ >= min_price_ && get_price_level(best_bid_)->total_volume == 0) {
            best_bid_--;
        }
        if (best_bid_ < min_price_) best_bid_ = 0;
    }

private:
    uint32_t min_price_;
    uint32_t max_price_;
    
    // A single contiguous array handling both bids and asks.
    std::pmr::vector<PriceLevel> price_levels_;

    // Avoid scanning the entire LOB array during crossing
    uint32_t best_bid_;
    uint32_t best_ask_;
};

} // namespace data
} // namespace matching_engine
