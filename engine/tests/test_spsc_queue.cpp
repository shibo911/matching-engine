#include <gtest/gtest.h>
#include <memory_resource>
#include "concurrency/SPSCQueue.hpp"
#include "memory/EngineMemory.hpp"
#include <thread>
#include <vector>

using namespace matching_engine::concurrency;
using namespace matching_engine::memory;

struct TestMessage
{
    uint64_t id;
    uint32_t value;
};

class SPSCQueueTest : public ::testing::Test
{
protected:
    static constexpr size_t CAPACITY = 16;

    EngineMemory memory{1024 * 1024};
    std::pmr::polymorphic_allocator<std::byte> alloc{memory.get_allocator()};
    SPSCQueue<TestMessage, CAPACITY> queue{alloc};
};

TEST_F(SPSCQueueTest, PopFromEmptyReturnsFalse)
{
    TestMessage msg;
    EXPECT_FALSE(queue.pop(msg));
}

TEST_F(SPSCQueueTest, PushAndPopSingleItem)
{
    TestMessage in{42, 100};
    EXPECT_TRUE(queue.push(in));

    TestMessage out;
    EXPECT_TRUE(queue.pop(out));
    EXPECT_EQ(out.id, 42u);
    EXPECT_EQ(out.value, 100u);
}

TEST_F(SPSCQueueTest, FIFOOrdering)
{
    for (uint64_t i = 0; i < 10; i++)
    {
        EXPECT_TRUE(queue.push({i, static_cast<uint32_t>(i * 10)}));
    }

    for (uint64_t i = 0; i < 10; i++)
    {
        TestMessage out;
        EXPECT_TRUE(queue.pop(out));
        EXPECT_EQ(out.id, i);
        EXPECT_EQ(out.value, i * 10);
    }
}

TEST_F(SPSCQueueTest, EmptyAfterDrain)
{
    queue.push({1, 10});
    queue.push({2, 20});

    TestMessage out;
    queue.pop(out);
    queue.pop(out);

    EXPECT_FALSE(queue.pop(out));
}

TEST_F(SPSCQueueTest, PushToCapacitySucceeds)
{
    for (size_t i = 0; i < CAPACITY; i++)
    {
        EXPECT_TRUE(queue.push({i, 0})) << "Failed at push " << i;
    }
}

TEST_F(SPSCQueueTest, PushBeyondCapacityReturnsFalse)
{
    for (size_t i = 0; i < CAPACITY; i++)
    {
        queue.push({i, 0});
    }

    EXPECT_FALSE(queue.push({999, 0}));
}

TEST_F(SPSCQueueTest, PopFreesSpaceForNewPush)
{
    for (size_t i = 0; i < CAPACITY; i++)
    {
        queue.push({i, 0});
    }
    EXPECT_FALSE(queue.push({999, 0}));

    TestMessage out;
    queue.pop(out);
    EXPECT_EQ(out.id, 0u);

    EXPECT_TRUE(queue.push({999, 0}));
}

TEST_F(SPSCQueueTest, WraparoundPreservesData)
{
    for (uint64_t round = 0; round < 5; round++)
    {
        for (uint64_t i = 0; i < CAPACITY; i++)
        {
            uint64_t val = round * CAPACITY + i;
            EXPECT_TRUE(queue.push({val, static_cast<uint32_t>(val)}));
        }
        for (uint64_t i = 0; i < CAPACITY; i++)
        {
            TestMessage out;
            EXPECT_TRUE(queue.pop(out));
            uint64_t expected = round * CAPACITY + i;
            EXPECT_EQ(out.id, expected);
        }
    }
}

TEST_F(SPSCQueueTest, ConcurrentProducerConsumer)
{
    static constexpr size_t NUM_ITEMS = 10000;

    EngineMemory mem2{1024 * 1024 * 2};
    auto alloc2 = mem2.get_allocator();
    SPSCQueue<TestMessage, 1024> concurrent_queue(alloc2);

    std::vector<uint64_t> received;
    received.reserve(NUM_ITEMS);

    std::thread producer([&]()
                         {
        for (uint64_t i = 0; i < NUM_ITEMS; i++) {
            while (!concurrent_queue.push({i, static_cast<uint32_t>(i)})) {
            }
        } });

    std::thread consumer([&]()
                         {
        uint64_t count = 0;
        while (count < NUM_ITEMS) {
            TestMessage out;
            if (concurrent_queue.pop(out)) {
                received.push_back(out.id);
                count++;
            }
        } });

    producer.join();
    consumer.join();

    ASSERT_EQ(received.size(), NUM_ITEMS);
    for (uint64_t i = 0; i < NUM_ITEMS; i++)
    {
        EXPECT_EQ(received[i], i) << "Out of order at index " << i;
    }
}
