#include <gtest/gtest.h>
#include <memory_resource>
#include "core/MatchingEngine.hpp"
#include "data/LimitOrderBook.hpp"
#include "data/CancelLookup.hpp"
#include "data/TradeEvent.hpp"
#include "concurrency/SPMCQueue.hpp"
#include "memory/EngineMemory.hpp"
#include "memory/ObjectPool.hpp"
#include <vector>

using namespace matching_engine;
using namespace matching_engine::data;
using namespace matching_engine::memory;
using namespace matching_engine::concurrency;
using namespace matching_engine::core;

static std::vector<TradeEvent> drain_trades(
    SPMCBroadcastQueue<TradeEvent, 1024> &egress, std::size_t &cursor)
{
    std::vector<TradeEvent> trades;
    TradeEvent ev;
    while (egress.pop(cursor, ev))
    {
        trades.push_back(ev);
    }
    return trades;
}

class MatchingEngineTest : public ::testing::Test
{
protected:
    static constexpr uint32_t MIN_PRICE = 10000;
    static constexpr uint32_t MAX_PRICE = 20000;
    static constexpr size_t MAX_ORDERS = 10000;

    EngineMemory memory{1024 * 1024 * 50}; // 50 MB
    std::pmr::polymorphic_allocator<std::byte> alloc{memory.get_allocator()};
    LimitOrderBook lob{MIN_PRICE, MAX_PRICE, alloc};
    CancelLookup cancel{MAX_ORDERS, alloc};
    ObjectPool<Order> pool{MAX_ORDERS, alloc};
    SPMCBroadcastQueue<TradeEvent, 1024> egress{alloc};
    MatchingEngine engine{&lob, &cancel, &pool, &egress};

    std::size_t egress_cursor = 0;

    Order *make_order(uint64_t id, uint32_t price, uint32_t qty, Side side)
    {
        Order *o = pool.allocate();
        o->id = id;
        o->price = price;
        o->quantity = qty;
        o->side = side;
        return o;
    }

    void place_resting(uint64_t id, uint32_t price, uint32_t qty, Side side)
    {
        Order *o = make_order(id, price, qty, side);
        lob.add_order(o);
        cancel.register_order(o);
    }

    void send_aggressive(uint64_t id, uint32_t price, uint32_t qty, Side side)
    {
        Order *o = make_order(id, price, qty, side);
        engine.process_order(o);
    }

    std::vector<TradeEvent> get_trades()
    {
        return drain_trades(egress, egress_cursor);
    }
};
TEST_F(MatchingEngineTest, BuyOrderRestsWhenNoAsks)
{
    send_aggressive(1, 15000, 100, Side::Buy);

    auto trades = get_trades();
    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(lob.best_bid(), 15000u);

    PriceLevel *level = lob.get_price_level(15000);
    EXPECT_EQ(level->total_volume, 100u);
}

TEST_F(MatchingEngineTest, SellOrderRestsWhenNoBids)
{
    send_aggressive(1, 15000, 100, Side::Sell);

    auto trades = get_trades();
    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(lob.best_ask(), 15000u);
}

TEST_F(MatchingEngineTest, BuyBelowBestAskRests)
{
    place_resting(1, 15100, 100, Side::Sell);
    send_aggressive(2, 15000, 100, Side::Buy);

    auto trades = get_trades();
    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(lob.best_bid(), 15000u);
    EXPECT_EQ(lob.best_ask(), 15100u);
}

TEST_F(MatchingEngineTest, SellAboveBestBidRests)
{
    place_resting(1, 15000, 100, Side::Buy);
    send_aggressive(2, 15100, 100, Side::Sell);

    auto trades = get_trades();
    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(lob.best_bid(), 15000u);
    EXPECT_EQ(lob.best_ask(), 15100u);
}

TEST_F(MatchingEngineTest, FullFillBuyAgainstAsk)
{
    place_resting(1, 15000, 100, Side::Sell);
    send_aggressive(2, 15000, 100, Side::Buy);

    auto trades = get_trades();
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].maker_order_id, 1u);
    EXPECT_EQ(trades[0].taker_order_id, 2u);
    EXPECT_EQ(trades[0].price, 15000u);
    EXPECT_EQ(trades[0].quantity, 100u);
    EXPECT_EQ(trades[0].side, Side::Buy);

    PriceLevel *level = lob.get_price_level(15000);
    EXPECT_EQ(level->total_volume, 0u);
}

TEST_F(MatchingEngineTest, FullFillSellAgainstBid)
{
    place_resting(1, 15000, 100, Side::Buy);
    send_aggressive(2, 15000, 100, Side::Sell);

    auto trades = get_trades();
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].maker_order_id, 1u);
    EXPECT_EQ(trades[0].taker_order_id, 2u);
    EXPECT_EQ(trades[0].price, 15000u);
    EXPECT_EQ(trades[0].quantity, 100u);
    EXPECT_EQ(trades[0].side, Side::Sell);
}

TEST_F(MatchingEngineTest, PartialFillAggressorLargerThanResting)
{
    place_resting(1, 15000, 50, Side::Sell);
    send_aggressive(2, 15000, 100, Side::Buy);

    auto trades = get_trades();
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 50u);

    EXPECT_EQ(lob.best_bid(), 15000u);
    PriceLevel *level = lob.get_price_level(15000);
    EXPECT_EQ(level->total_volume, 50u);
}

TEST_F(MatchingEngineTest, PartialFillRestingLargerThanAggressor)
{
    place_resting(1, 15000, 200, Side::Sell);
    send_aggressive(2, 15000, 50, Side::Buy);

    auto trades = get_trades();
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].quantity, 50u);

    PriceLevel *level = lob.get_price_level(15000);
    EXPECT_EQ(level->total_volume, 150u);
    EXPECT_EQ(level->orders.head()->quantity, 150u);

    EXPECT_EQ(lob.best_bid(), 0u);
}

TEST_F(MatchingEngineTest, PriceTimePriorityFIFO)
{
    place_resting(10, 15000, 30, Side::Sell);
    place_resting(11, 15000, 30, Side::Sell);
    place_resting(12, 15000, 30, Side::Sell);

    send_aggressive(20, 15000, 60, Side::Buy);

    auto trades = get_trades();
    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].maker_order_id, 10u);
    EXPECT_EQ(trades[0].quantity, 30u);
    EXPECT_EQ(trades[1].maker_order_id, 11u);
    EXPECT_EQ(trades[1].quantity, 30u);

    PriceLevel *level = lob.get_price_level(15000);
    EXPECT_EQ(level->total_volume, 30u);
    EXPECT_EQ(level->orders.head()->id, 12u);
}

TEST_F(MatchingEngineTest, BuyMatchesBestAskFirst)
{
    place_resting(1, 15100, 50, Side::Sell);
    place_resting(2, 15000, 50, Side::Sell);

    send_aggressive(3, 15200, 80, Side::Buy);

    auto trades = get_trades();
    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].price, 15000u);
    EXPECT_EQ(trades[0].quantity, 50u);
    EXPECT_EQ(trades[1].price, 15100u);
    EXPECT_EQ(trades[1].quantity, 30u);
}

TEST_F(MatchingEngineTest, SellMatchesBestBidFirst)
{
    place_resting(1, 14900, 50, Side::Buy);
    place_resting(2, 15000, 50, Side::Buy);
    send_aggressive(3, 14800, 80, Side::Sell);

    auto trades = get_trades();
    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].price, 15000u);
    EXPECT_EQ(trades[1].price, 14900u);
}

TEST_F(MatchingEngineTest, AggressorSweepsMultipleLevels)
{
    place_resting(1, 15000, 100, Side::Sell);
    place_resting(2, 15001, 100, Side::Sell);
    place_resting(3, 15002, 100, Side::Sell);
    send_aggressive(10, 15005, 250, Side::Buy);

    auto trades = get_trades();
    ASSERT_EQ(trades.size(), 3u);
    EXPECT_EQ(trades[0].price, 15000u);
    EXPECT_EQ(trades[0].quantity, 100u);
    EXPECT_EQ(trades[1].price, 15001u);
    EXPECT_EQ(trades[1].quantity, 100u);
    EXPECT_EQ(trades[2].price, 15002u);
    EXPECT_EQ(trades[2].quantity, 50u);

    EXPECT_EQ(lob.get_price_level(15002)->total_volume, 50u);
    EXPECT_EQ(lob.best_ask(), 15002u);
}

TEST_F(MatchingEngineTest, BuyerGetsPriceImprovement)
{
    place_resting(1, 14900, 100, Side::Sell);

    send_aggressive(2, 15100, 100, Side::Buy);

    auto trades = get_trades();
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].price, 14900u);
}

TEST_F(MatchingEngineTest, SellerGetsPriceImprovement)
{
    place_resting(1, 15200, 100, Side::Buy);

    send_aggressive(2, 14900, 100, Side::Sell);

    auto trades = get_trades();
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].price, 15200u);
}
TEST_F(MatchingEngineTest, CancelledOrderIsNotMatched)
{
    place_resting(1, 15000, 100, Side::Sell);

    Order *to_cancel = cancel.get_order(1);
    ASSERT_NE(to_cancel, nullptr);
    lob.remove_order(to_cancel);
    cancel.deregister_order(1);
    pool.deallocate(to_cancel);
    send_aggressive(2, 15000, 100, Side::Buy);

    auto trades = get_trades();
    EXPECT_TRUE(trades.empty());
    EXPECT_EQ(lob.best_bid(), 15000u);
}

TEST_F(MatchingEngineTest, BestAskAdvancesAfterLevelDrained)
{
    place_resting(1, 15000, 50, Side::Sell);
    place_resting(2, 15001, 50, Side::Sell);

    EXPECT_EQ(lob.best_ask(), 15000u);

    send_aggressive(3, 15000, 50, Side::Buy);

    EXPECT_EQ(lob.best_ask(), 15001u);
}

TEST_F(MatchingEngineTest, BestBidRetreatsAfterLevelDrained)
{
    place_resting(1, 15000, 50, Side::Buy);
    place_resting(2, 14999, 50, Side::Buy);

    EXPECT_EQ(lob.best_bid(), 15000u);

    send_aggressive(3, 15000, 50, Side::Sell);

    EXPECT_EQ(lob.best_bid(), 14999u);
}

TEST_F(MatchingEngineTest, TradeEventHasIncrementingId)
{
    place_resting(1, 15000, 50, Side::Sell);
    place_resting(2, 15001, 50, Side::Sell);

    send_aggressive(10, 15001, 100, Side::Buy);

    auto trades = get_trades();
    ASSERT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].trade_id, 1u);
    EXPECT_EQ(trades[1].trade_id, 2u);
}

TEST_F(MatchingEngineTest, TradeEventRecordsMakerAndTaker)
{
    place_resting(100, 15000, 50, Side::Sell);
    send_aggressive(200, 15000, 50, Side::Buy);

    auto trades = get_trades();
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].maker_order_id, 100u);
    EXPECT_EQ(trades[0].taker_order_id, 200u);
}

TEST_F(MatchingEngineTest, ZeroQuantityOrderDoesNotCrash)
{
    send_aggressive(1, 15000, 0, Side::Buy);
    auto trades = get_trades();
    EXPECT_TRUE(trades.empty());
}

TEST_F(MatchingEngineTest, ManyOrdersStressTest)
{
    for (uint64_t i = 1; i <= 1000; i++)
    {
        place_resting(i, 15000 + (i % 50), 10, Side::Sell);
    }

    send_aggressive(2000, 15050, 5000, Side::Buy);

    auto trades = get_trades();
    EXPECT_GT(trades.size(), 0u);

    std::vector<uint64_t> ids;
    for (auto &t : trades)
    {
        ids.push_back(t.trade_id);
    }
    std::sort(ids.begin(), ids.end());
    auto it = std::adjacent_find(ids.begin(), ids.end());
    EXPECT_EQ(it, ids.end()) << "Duplicate trade ID found: " << *it;
}

TEST_F(MatchingEngineTest, BothSidesCanRestSimultaneously)
{
    place_resting(1, 14000, 100, Side::Buy);
    place_resting(2, 16000, 100, Side::Sell);

    EXPECT_EQ(lob.best_bid(), 14000u);
    EXPECT_EQ(lob.best_ask(), 16000u);

    auto trades = get_trades();
    EXPECT_TRUE(trades.empty());
}
