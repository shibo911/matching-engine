#include <gtest/gtest.h>
#include "data/Order.hpp"
#include "data/PriceLevel.hpp"
#include "data/TradeEvent.hpp"
#include "data/IntrusiveOrderList.hpp"

using namespace matching_engine::data;

TEST(OrderPacking, SizeIs40Bytes) {
    EXPECT_EQ(sizeof(Order), 40);
}

TEST(OrderPacking, AlignmentIs8Bytes) {
    EXPECT_EQ(alignof(Order), 8);
}

TEST(PriceLevelPacking, SizeIs24Bytes) {
    EXPECT_EQ(sizeof(PriceLevel), 24);
}

TEST(TradeEventPacking, SizeIs40Bytes) {
    EXPECT_EQ(sizeof(TradeEvent), 40);
}

TEST(Order, DefaultConstruction) {
    Order o;
    EXPECT_EQ(o.next, nullptr);
    EXPECT_EQ(o.prev, nullptr);
    EXPECT_EQ(o.id, 0u);
    EXPECT_EQ(o.price, 0u);
    EXPECT_EQ(o.quantity, 0u);
    EXPECT_EQ(o.side, Side::Buy);
}
class IntrusiveOrderListTest : public ::testing::Test {
protected:
    IntrusiveOrderList list;
    Order orders[5];

    void SetUp() override {
        for (int i = 0; i < 5; i++) {
            orders[i].id = i + 1;
            orders[i].price = 15000;
            orders[i].quantity = 100;
            orders[i].next = nullptr;
            orders[i].prev = nullptr;
        }
    }
};

TEST_F(IntrusiveOrderListTest, EmptyListState) {
    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.head(), nullptr);
    EXPECT_EQ(list.tail(), nullptr);
}

TEST_F(IntrusiveOrderListTest, PushBackSingleElement) {
    list.push_back(&orders[0]);
    EXPECT_FALSE(list.empty());
    EXPECT_EQ(list.head(), &orders[0]);
    EXPECT_EQ(list.tail(), &orders[0]);
    EXPECT_EQ(list.head()->next, nullptr);
    EXPECT_EQ(list.head()->prev, nullptr);
}

TEST_F(IntrusiveOrderListTest, PushBackMultipleElements) {
    list.push_back(&orders[0]);
    list.push_back(&orders[1]);
    list.push_back(&orders[2]);

    EXPECT_EQ(list.head(), &orders[0]);
    EXPECT_EQ(list.tail(), &orders[2]);

    EXPECT_EQ(orders[0].next, &orders[1]);
    EXPECT_EQ(orders[1].next, &orders[2]);
    EXPECT_EQ(orders[2].next, nullptr);

    EXPECT_EQ(orders[2].prev, &orders[1]);
    EXPECT_EQ(orders[1].prev, &orders[0]);
    EXPECT_EQ(orders[0].prev, nullptr);
}

TEST_F(IntrusiveOrderListTest, PushFrontSingleElement) {
    list.push_front(&orders[0]);
    EXPECT_EQ(list.head(), &orders[0]);
    EXPECT_EQ(list.tail(), &orders[0]);
}

TEST_F(IntrusiveOrderListTest, PushFrontMultipleElements) {
    list.push_front(&orders[0]);
    list.push_front(&orders[1]);
    list.push_front(&orders[2]);
    EXPECT_EQ(list.head(), &orders[2]);
    EXPECT_EQ(list.tail(), &orders[0]);
}

TEST_F(IntrusiveOrderListTest, RemoveMiddleElement) {
    list.push_back(&orders[0]);
    list.push_back(&orders[1]);
    list.push_back(&orders[2]);

    list.remove(&orders[1]);

    EXPECT_EQ(list.head(), &orders[0]);
    EXPECT_EQ(list.tail(), &orders[2]);
    EXPECT_EQ(orders[0].next, &orders[2]);
    EXPECT_EQ(orders[2].prev, &orders[0]);

    EXPECT_EQ(orders[1].next, nullptr);
    EXPECT_EQ(orders[1].prev, nullptr);
}

TEST_F(IntrusiveOrderListTest, RemoveHeadElement) {
    list.push_back(&orders[0]);
    list.push_back(&orders[1]);

    list.remove(&orders[0]);

    EXPECT_EQ(list.head(), &orders[1]);
    EXPECT_EQ(list.tail(), &orders[1]);
    EXPECT_EQ(orders[1].prev, nullptr);
}

TEST_F(IntrusiveOrderListTest, RemoveTailElement) {
    list.push_back(&orders[0]);
    list.push_back(&orders[1]);

    list.remove(&orders[1]);

    EXPECT_EQ(list.head(), &orders[0]);
    EXPECT_EQ(list.tail(), &orders[0]);
    EXPECT_EQ(orders[0].next, nullptr);
}

TEST_F(IntrusiveOrderListTest, RemoveOnlyElement) {
    list.push_back(&orders[0]);
    list.remove(&orders[0]);

    EXPECT_TRUE(list.empty());
    EXPECT_EQ(list.head(), nullptr);
    EXPECT_EQ(list.tail(), nullptr);
}

TEST_F(IntrusiveOrderListTest, PopFrontFromMultipleElements) {
    list.push_back(&orders[0]);
    list.push_back(&orders[1]);
    list.push_back(&orders[2]);

    Order* popped = list.pop_front();
    EXPECT_EQ(popped, &orders[0]);
    EXPECT_EQ(list.head(), &orders[1]);

    popped = list.pop_front();
    EXPECT_EQ(popped, &orders[1]);
    EXPECT_EQ(list.head(), &orders[2]);

    popped = list.pop_front();
    EXPECT_EQ(popped, &orders[2]);
    EXPECT_TRUE(list.empty());
}

TEST_F(IntrusiveOrderListTest, PopFrontFromEmptyList) {
    EXPECT_EQ(list.pop_front(), nullptr);
}

TEST_F(IntrusiveOrderListTest, RemoveAllElementsInReverseOrder) {
    for (int i = 0; i < 5; i++) {
        list.push_back(&orders[i]);
    }

    for (int i = 4; i >= 0; i--) {
        list.remove(&orders[i]);
    }

    EXPECT_TRUE(list.empty());
}
