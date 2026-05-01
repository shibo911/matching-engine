#pragma once
#include "data/Order.hpp"
#include "data/LimitOrderBook.hpp"
#include "data/CancelLookup.hpp"
#include "data/TradeEvent.hpp"
#include "concurrency/SPMCQueue.hpp"
#include "memory/ObjectPool.hpp"
#include <algorithm>

namespace matching_engine {
namespace core {

class MatchingEngine {
public:
    MatchingEngine(
        data::LimitOrderBook* lob,
        data::CancelLookup* cancel_lookup,
        memory::ObjectPool<data::Order>* order_pool,
        concurrency::SPMCBroadcastQueue<data::TradeEvent, 1024>* egress_queue)
        : lob_(lob), 
          cancel_lookup_(cancel_lookup), 
          order_pool_(order_pool), 
          egress_queue_(egress_queue),
          trade_counter_(0) {}

    // The core algorithmic entry point for crossing aggressive orders
    inline void process_order(data::Order* aggressive_order) {
        if (aggressive_order->side == data::Side::Buy) {
            match_buy(aggressive_order);
        } else {
            match_sell(aggressive_order);
        }
    }

private:
    data::LimitOrderBook* lob_;
    data::CancelLookup* cancel_lookup_;
    memory::ObjectPool<data::Order>* order_pool_;
    concurrency::SPMCBroadcastQueue<data::TradeEvent, 1024>* egress_queue_;
    uint64_t trade_counter_;

    inline void match_buy(data::Order* buy_order) {
        // 1. Scan resting asks from Best Ask upwards
        while (buy_order->quantity > 0 && lob_->best_ask() <= buy_order->price) {
            data::PriceLevel* level = lob_->get_price_level(lob_->best_ask());
            
            // 2. Traverse the Intrusive Linked List at this exact price level (Price-Time Priority)
            data::Order* resting_ask = level->orders.head();
            
            while (resting_ask != nullptr && buy_order->quantity > 0) {
                uint32_t fill_qty = (std::min)(buy_order->quantity, resting_ask->quantity);
                uint32_t fill_price = resting_ask->price; // Price improvement goes to aggressor

                // 3. Emit Trade Event instantly via the Wait-Free Broadcast Queue
                egress_queue_->push(data::TradeEvent{
                    .trade_id        = ++trade_counter_,
                    .maker_order_id  = resting_ask->id,
                    .taker_order_id  = buy_order->id,
                    .timestamp       = 0,
                    .price           = fill_price,
                    .quantity        = fill_qty,
                    .side            = data::Side::Buy,
                    .reserved        = {0, 0, 0}
                });

                buy_order->quantity -= fill_qty;
                resting_ask->quantity -= fill_qty;
                level->total_volume -= fill_qty;

                // 4. Handle resting order fill (Deallocation)
                if (resting_ask->quantity == 0) {
                    data::Order* to_delete = resting_ask;
                    resting_ask = resting_ask->next; // Advance pointers before unlinking
                    
                    level->orders.remove(to_delete);
                    cancel_lookup_->deregister_order(to_delete->id);
                    order_pool_->deallocate(to_delete);
                } else {
                    // Resting order only partially filled; Aggressor is completely exhausted.
                    break; 
                }
            }

            // 5. If we completely emptied this price level, bump Best Ask upward
            if (level->total_volume == 0) {
                lob_->advance_best_ask();
            }
        }

        // 6. If the aggressive order still has leftover quantity, bump-allocate it to rest in the LOB
        if (buy_order->quantity > 0) {
            lob_->add_order(buy_order);
            cancel_lookup_->register_order(buy_order);
        } else {
            // Aggressor is 100% filled, safely recycle its memory
            order_pool_->deallocate(buy_order);
        }
    }

    inline void match_sell(data::Order* sell_order) {
        // 1. Scan resting bids from Best Bid downwards
        while (sell_order->quantity > 0 && lob_->best_bid() >= sell_order->price && lob_->best_bid() > 0) {
            data::PriceLevel* level = lob_->get_price_level(lob_->best_bid());
            data::Order* resting_bid = level->orders.head();
            
            while (resting_bid != nullptr && sell_order->quantity > 0) {
                uint32_t fill_qty = (std::min)(sell_order->quantity, resting_bid->quantity);
                uint32_t fill_price = resting_bid->price; 

                egress_queue_->push(data::TradeEvent{
                    .trade_id        = ++trade_counter_,
                    .maker_order_id  = resting_bid->id,
                    .taker_order_id  = sell_order->id,
                    .timestamp       = 0,
                    .price           = fill_price,
                    .quantity        = fill_qty,
                    .side            = data::Side::Sell,
                    .reserved        = {0, 0, 0}
                });

                sell_order->quantity -= fill_qty;
                resting_bid->quantity -= fill_qty;
                level->total_volume -= fill_qty;

                if (resting_bid->quantity == 0) {
                    data::Order* to_delete = resting_bid;
                    resting_bid = resting_bid->next; 
                    
                    level->orders.remove(to_delete);
                    cancel_lookup_->deregister_order(to_delete->id);
                    order_pool_->deallocate(to_delete);
                } else {
                    break; 
                }
            }

            if (level->total_volume == 0) {
                lob_->retreat_best_bid();
            }
        }

        if (sell_order->quantity > 0) {
            lob_->add_order(sell_order);
            cancel_lookup_->register_order(sell_order);
        } else {
            order_pool_->deallocate(sell_order);
        }
    }
};

} // namespace core
} // namespace matching_engine
