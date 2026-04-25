#pragma once
#include <map>
#include <list>
#include <mutex>
#include <cstdint>
#include <algorithm>
#include "data/Order.hpp"

namespace matching_engine {
namespace core {

// A standard std::map and std::mutex engine written by a junior developer.
// Used exclusively for benchmarking to prove the latency impact of heap 
// allocations, STL tree-rotations, and cache misses.
class NaiveEngine {
public:
    void add_order(const data::Order& order) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (order.side == data::Side::Buy) {
            bids_[order.price].push_back(order);
        } else {
            asks_[order.price].push_back(order);
        }
        order_map_[order.id] = order; // Expensive map tree allocation
    }

    void cancel_order(uint64_t order_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = order_map_.find(order_id); // Expensive tree traversal
        if (it == order_map_.end()) return;

        data::Order order = it->second;
        order_map_.erase(it); // Heap deallocation

        if (order.side == data::Side::Buy) {
            auto& level = bids_[order.price];
            level.remove_if([=](const data::Order& o) { return o.id == order_id; }); // Linear O(N) scan!
            if (level.empty()) bids_.erase(order.price);
        } else {
            auto& level = asks_[order.price];
            level.remove_if([=](const data::Order& o) { return o.id == order_id; });
            if (level.empty()) asks_.erase(order.price);
        }
    }

    void match_order(data::Order order) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (order.side == data::Side::Buy) {
            auto ask_it = asks_.begin();
            while (ask_it != asks_.end() && order.quantity > 0 && ask_it->first <= order.price) {
                auto& level = ask_it->second;
                auto order_it = level.begin();
                while (order_it != level.end() && order.quantity > 0) {
                    uint32_t fill_qty = (std::min)(order.quantity, order_it->quantity);
                    order.quantity -= fill_qty;
                    order_it->quantity -= fill_qty;
                    
                    if (order_it->quantity == 0) {
                        order_map_.erase(order_it->id);
                        order_it = level.erase(order_it); // O(N) linked list pointer rewrite + heap deallocation
                    } else {
                        break;
                    }
                }
                if (level.empty()) {
                    ask_it = asks_.erase(ask_it);
                } else {
                    ++ask_it;
                }
            }
            if (order.quantity > 0) {
                bids_[order.price].push_back(order);
                order_map_[order.id] = order;
            }
        } else {
            auto bid_it = bids_.rbegin();
            while (bid_it != bids_.rend() && order.quantity > 0 && bid_it->first >= order.price) {
                auto& level = bid_it->second;
                auto order_it = level.begin();
                while (order_it != level.end() && order.quantity > 0) {
                    uint32_t fill_qty = (std::min)(order.quantity, order_it->quantity);
                    order.quantity -= fill_qty;
                    order_it->quantity -= fill_qty;
                    
                    if (order_it->quantity == 0) {
                        order_map_.erase(order_it->id);
                        order_it = level.erase(order_it);
                    } else {
                        break;
                    }
                }
                if (level.empty()) {
                    bid_it = decltype(bid_it)(bids_.erase(std::next(bid_it).base()));
                } else {
                    ++bid_it;
                }
            }
            if (order.quantity > 0) {
                asks_[order.price].push_back(order);
                order_map_[order.id] = order;
            }
        }
    }

private:
    std::mutex mutex_;
    // Cache-thrashing structures
    std::map<uint32_t, std::list<data::Order>> bids_;
    std::map<uint32_t, std::list<data::Order>> asks_;
    std::map<uint64_t, data::Order> order_map_;
};

} // namespace core
} // namespace matching_engine
