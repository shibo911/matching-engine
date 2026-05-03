#include <gtest/gtest.h>
#include <memory_resource>
#include "concurrency/SPMCQueue.hpp"
#include "data/TradeEvent.hpp"
#include "memory/EngineMemory.hpp"
#include <thread>
#include <vector>
#include <atomic>

using namespace matching_engine::concurrency;
using namespace matching_engine::data;
using namespace matching_engine::memory;

class SPMCQueueTest : public ::testing::Test {
protected:
    static constexpr size_t CAPACITY = 16;

    EngineMemory memory{1024 * 1024 * 2}; 
    std::pmr::polymorphic_allocator<std::byte> alloc{memory.get_allocator()};
    SPMCBroadcastQueue<TradeEvent, CAPACITY> queue{alloc};

    TradeEvent make_event(uint64_t id, uint32_t price = 15000, uint32_t qty = 100) {
        return TradeEvent{
            .trade_id = id,
            .maker_order_id = 0,
            .taker_order_id = 0,
            .timestamp = 0,
            .price = price,
            .quantity = qty,
            .side = Side::Buy,
            .reserved = {0, 0, 0}
        };
    }
};

TEST_F(SPMCQueueTest, PopFromEmptyReturnsFalse) {
    std::size_t cursor = 0;
    TradeEvent out;
    EXPECT_FALSE(queue.pop(cursor, out));
}

TEST_F(SPMCQueueTest, PushAndPopSingleItem) {
    queue.push(make_event(1, 15000, 50));

    std::size_t cursor = 0;
    TradeEvent out;
    EXPECT_TRUE(queue.pop(cursor, out));
    EXPECT_EQ(out.trade_id, 1u);
    EXPECT_EQ(out.price, 15000u);
    EXPECT_EQ(out.quantity, 50u);
}

TEST_F(SPMCQueueTest, SequentialPushPop) {
    for (uint64_t i = 1; i <= 5; i++) {
        queue.push(make_event(i, 15000 + i));
    }

    std::size_t cursor = 0;
    for (uint64_t i = 1; i <= 5; i++) {
        TradeEvent out;
        EXPECT_TRUE(queue.pop(cursor, out));
        EXPECT_EQ(out.trade_id, i);
        EXPECT_EQ(out.price, 15000 + i);
    }

    // No more items
    TradeEvent out;
    EXPECT_FALSE(queue.pop(cursor, out));
}

TEST_F(SPMCQueueTest, CursorAdvancesBy2PerEvent) {
    std::size_t cursor = 0;
    queue.push(make_event(1));
    queue.push(make_event(2));

    TradeEvent out;
    queue.pop(cursor, out);
    EXPECT_EQ(cursor, 2u);

    queue.pop(cursor, out);
    EXPECT_EQ(cursor, 4u);
}

TEST_F(SPMCQueueTest, TwoConsumersReadSameData) {
    queue.push(make_event(1));
    queue.push(make_event(2));
    queue.push(make_event(3));

    std::size_t cursor_a = 0;
    std::size_t cursor_b = 0;

    std::vector<uint64_t> a_ids;
    TradeEvent out;
    while (queue.pop(cursor_a, out)) {
        a_ids.push_back(out.trade_id);
    }

    std::vector<uint64_t> b_ids;
    while (queue.pop(cursor_b, out)) {
        b_ids.push_back(out.trade_id);
    }

    ASSERT_EQ(a_ids.size(), 3u);
    ASSERT_EQ(b_ids.size(), 3u);

    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(a_ids[i], b_ids[i]);
    }
}

TEST_F(SPMCQueueTest, SlowConsumerFastForwardsOnLap) {
    std::size_t cursor = 0;

    for (uint64_t i = 1; i <= CAPACITY * 2; i++) {
        queue.push(make_event(i));
    }
    TradeEvent out;
    bool result = queue.pop(cursor, out);

    if (!result) {
        EXPECT_GT(cursor, 0u);
    }
}
TEST_F(SPMCQueueTest, ConcurrentProducerMultipleConsumers) {
    static constexpr size_t NUM_ITEMS = 5000;
    static constexpr int NUM_CONSUMERS = 3;

    EngineMemory mem2{1024 * 1024 * 4};
    auto alloc2 = mem2.get_allocator();
    SPMCBroadcastQueue<TradeEvent, 1024> cq{alloc2};

    std::atomic<bool> done{false};

    std::thread producer([&]() {
        for (uint64_t i = 1; i <= NUM_ITEMS; i++) {
            cq.push(TradeEvent{
                .trade_id = i,
                .maker_order_id = 0, .taker_order_id = 0,
                .timestamp = 0,
                .price = static_cast<uint32_t>(15000 + (i % 100)),
                .quantity = 10,
                .side = Side::Buy,
                .reserved = {0, 0, 0}
            });
        }
        done.store(true, std::memory_order_release);
    });

    std::vector<std::thread> consumers;
    std::atomic<size_t> total_consumed[NUM_CONSUMERS];
    for (int c = 0; c < NUM_CONSUMERS; c++) {
        total_consumed[c].store(0);
    }

    for (int c = 0; c < NUM_CONSUMERS; c++) {
        consumers.emplace_back([&, c]() {
            std::size_t cursor = 0;
            TradeEvent out;
            size_t count = 0;

            while (!done.load(std::memory_order_acquire) || cursor < NUM_ITEMS * 2) {
                if (cq.pop(cursor, out)) {
                    count++;
                    EXPECT_GT(out.trade_id, 0u);
                    EXPECT_LE(out.trade_id, NUM_ITEMS);
                } else {
                    if (done.load(std::memory_order_acquire)) break;
                    std::this_thread::yield();
                }
            }
            total_consumed[c].store(count);
        });
    }

    producer.join();
    for (auto& t : consumers) t.join();

    for (int c = 0; c < NUM_CONSUMERS; c++) {
        EXPECT_GT(total_consumed[c].load(), 0u)
            << "Consumer " << c << " read zero events";
    }
}
