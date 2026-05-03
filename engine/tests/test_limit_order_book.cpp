#include <gtest/gtest.h>
#include <memory_resource>
#include "data/LimitOrderBook.hpp"
#include "data/Order.hpp"
#include "memory/EngineMemory.hpp"

using namespace matching_engine::data;
using namespace matching_engine::memory;

class LimitOrderBookTest : public ::testing::Test {
protected:
    static constexpr uint32_t MIN_PRICE = 10000;
    static constexpr uint32_t MAX_PRICE = 20000;

    EngineMemory memory{1024 * 1024 * 10}; // 10 MB for tests
    std::pmr::polymorphic_allocator<std::byte> alloc{memory.get_allocator()};
    LimitOrderBook lob{MIN_PRICE, MAX_PRICE, alloc};

    Order make_order(uint64_t id, uint32_t price, uint32_t qty, Side side) {
        Order o;
        o.id = id;
        o.price = price;
        o.quantity = qty;
        o.side = side;
        o.next = nullptr;
        o.prev = nullptr;
        return o;
    }
};
TEST_F(LimitOrderBookTest, InitialBestBidIsZero) {
    EXPECT_EQ(lob.best_bid(), 0u);
}

TEST_F(LimitOrderBookTest, InitialBestAskIsAboveMax) {
    EXPECT_GT(lob.best_ask(), MAX_PRICE);
}

TEST_F(LimitOrderBookTest, PriceRange) {
    EXPECT_EQ(lob.min_price(), MIN_PRICE);
    EXPECT_EQ(lob.max_price(), MAX_PRICE);
}

TEST_F(LimitOrderBookTest, AddSingleBid) {
    Order bid = make_order(1, 15000, 100, Side::Buy);
    lob.add_order(&bid);

    EXPECT_EQ(lob.best_bid(), 15000u);
    PriceLevel* level = lob.get_price_level(15000);
    ASSERT_NE(level, nullptr);
    EXPECT_EQ(level->total_volume, 100u);
    EXPECT_EQ(level->orders.head(), &bid);
}

TEST_F(LimitOrderBookTest, AddSingleAsk) {
    Order ask = make_order(1, 15500, 200, Side::Sell);
    lob.add_order(&ask);

    EXPECT_EQ(lob.best_ask(), 15500u);
    PriceLevel* level = lob.get_price_level(15500);
    ASSERT_NE(level, nullptr);
    EXPECT_EQ(level->total_volume, 200u);
}

TEST_F(LimitOrderBookTest, BestBidTracksHighest) {
    Order bid1 = make_order(1, 15000, 100, Side::Buy);
    Order bid2 = make_order(2, 15100, 100, Side::Buy);
    Order bid3 = make_order(3, 14900, 100, Side::Buy);

    lob.add_order(&bid1);
    EXPECT_EQ(lob.best_bid(), 15000u);

    lob.add_order(&bid2);
    EXPECT_EQ(lob.best_bid(), 15100u);

    lob.add_order(&bid3);
    EXPECT_EQ(lob.best_bid(), 15100u);
}

TEST_F(LimitOrderBookTest, BestAskTracksLowest) {
    Order ask1 = make_order(1, 15500, 100, Side::Sell);
    Order ask2 = make_order(2, 15400, 100, Side::Sell);
    Order ask3 = make_order(3, 15600, 100, Side::Sell);

    lob.add_order(&ask1);
    EXPECT_EQ(lob.best_ask(), 15500u);

    lob.add_order(&ask2);
    EXPECT_EQ(lob.best_ask(), 15400u);

    lob.add_order(&ask3);
    EXPECT_EQ(lob.best_ask(), 15400u);
}

TEST_F(LimitOrderBookTest, MultipleOrdersSamePriceLevel) {
    Order bid1 = make_order(1, 15000, 100, Side::Buy);
    Order bid2 = make_order(2, 15000, 200, Side::Buy);

    lob.add_order(&bid1);
    lob.add_order(&bid2);

    PriceLevel* level = lob.get_price_level(15000);
    EXPECT_EQ(level->total_volume, 300u);
    EXPECT_EQ(level->orders.head(), &bid1); 
    EXPECT_EQ(level->orders.tail(), &bid2);
}

TEST_F(LimitOrderBookTest, RemoveOrderUpdatesVolume) {
    Order bid = make_order(1, 15000, 100, Side::Buy);
    lob.add_order(&bid);
    lob.remove_order(&bid);

    PriceLevel* level = lob.get_price_level(15000);
    EXPECT_EQ(level->total_volume, 0u);
    EXPECT_TRUE(level->orders.empty());
}

TEST_F(LimitOrderBookTest, RemoveBestBidRetreats) {
    Order bid1 = make_order(1, 15000, 100, Side::Buy);
    Order bid2 = make_order(2, 14900, 100, Side::Buy);

    lob.add_order(&bid1);
    lob.add_order(&bid2);
    EXPECT_EQ(lob.best_bid(), 15000u);

    lob.remove_order(&bid1);
    EXPECT_EQ(lob.best_bid(), 14900u);
}

TEST_F(LimitOrderBookTest, RemoveBestAskAdvances) {
    Order ask1 = make_order(1, 15400, 100, Side::Sell);
    Order ask2 = make_order(2, 15500, 100, Side::Sell);

    lob.add_order(&ask1);
    lob.add_order(&ask2);
    EXPECT_EQ(lob.best_ask(), 15400u);

    lob.remove_order(&ask1);
    EXPECT_EQ(lob.best_ask(), 15500u);
}

TEST_F(LimitOrderBookTest, RemoveAllBidsResetsBestBid) {
    Order bid = make_order(1, 15000, 100, Side::Buy);
    lob.add_order(&bid);
    lob.remove_order(&bid);
    EXPECT_EQ(lob.best_bid(), 0u);
}

TEST_F(LimitOrderBookTest, RemoveAllAsksResetsBestAsk) {
    Order ask = make_order(1, 15000, 100, Side::Sell);
    lob.add_order(&ask);
    lob.remove_order(&ask);
    EXPECT_GT(lob.best_ask(), MAX_PRICE);
}
TEST_F(LimitOrderBookTest, OrderAtMinPrice) {
    Order bid = make_order(1, MIN_PRICE, 100, Side::Buy);
    lob.add_order(&bid);

    EXPECT_EQ(lob.best_bid(), MIN_PRICE);
    PriceLevel* level = lob.get_price_level(MIN_PRICE);
    EXPECT_EQ(level->total_volume, 100u);
}

TEST_F(LimitOrderBookTest, OrderAtMaxPrice) {
    Order ask = make_order(1, MAX_PRICE, 100, Side::Sell);
    lob.add_order(&ask);

    EXPECT_EQ(lob.best_ask(), MAX_PRICE);
    PriceLevel* level = lob.get_price_level(MAX_PRICE);
    EXPECT_EQ(level->total_volume, 100u);
}

TEST_F(LimitOrderBookTest, OutOfRangePriceReturnsNull) {
    EXPECT_EQ(lob.get_price_level(MIN_PRICE - 1), nullptr);
    EXPECT_EQ(lob.get_price_level(MAX_PRICE + 1), nullptr);
    EXPECT_EQ(lob.get_price_level(0), nullptr);
}

TEST_F(LimitOrderBookTest, InvalidPriceRangeThrows) {
    EXPECT_THROW(
        LimitOrderBook(20000, 10000, alloc), 
        std::invalid_argument
    );
}

TEST_F(LimitOrderBookTest, RemoveOneOfTwoAtSameLevel) {
    Order bid1 = make_order(1, 15000, 100, Side::Buy);
    Order bid2 = make_order(2, 15000, 200, Side::Buy);

    lob.add_order(&bid1);
    lob.add_order(&bid2);
    lob.remove_order(&bid1);

    PriceLevel* level = lob.get_price_level(15000);
    EXPECT_EQ(level->total_volume, 200u);
    EXPECT_EQ(level->orders.head(), &bid2);
    EXPECT_EQ(lob.best_bid(), 15000u);
}
