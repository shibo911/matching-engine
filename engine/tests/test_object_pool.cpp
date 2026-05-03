#include <gtest/gtest.h>
#include <memory_resource>
#include "memory/ObjectPool.hpp"
#include "memory/EngineMemory.hpp"
#include "data/Order.hpp"

using namespace matching_engine::memory;
using namespace matching_engine::data;

class ObjectPoolTest : public ::testing::Test {
protected:
    static constexpr size_t POOL_CAPACITY = 100;

    EngineMemory memory{1024 * 1024 * 5}; // 5 MB
    std::pmr::polymorphic_allocator<std::byte> alloc{memory.get_allocator()};
    ObjectPool<Order> pool{POOL_CAPACITY, alloc};
};

TEST_F(ObjectPoolTest, AllocateReturnsNonNull) {
    Order* o = pool.allocate();
    ASSERT_NE(o, nullptr);
}

TEST_F(ObjectPoolTest, AllocatedOrderHasDefaultValues) {
    Order* o = pool.allocate();
    ASSERT_NE(o, nullptr);
    EXPECT_EQ(o->next, nullptr);
    EXPECT_EQ(o->prev, nullptr);
    EXPECT_EQ(o->id, 0u);
    EXPECT_EQ(o->price, 0u);
    EXPECT_EQ(o->quantity, 0u);
}

TEST_F(ObjectPoolTest, MultipleAllocationsReturnDistinctPointers) {
    Order* a = pool.allocate();
    Order* b = pool.allocate();
    Order* c = pool.allocate();

    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);
    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_NE(a, c);
}

TEST_F(ObjectPoolTest, DeallocateAndReuseFromFreeList) {
    Order* first = pool.allocate();
    ASSERT_NE(first, nullptr);
    first->id = 42;
    first->price = 15000;

    pool.deallocate(first);

    Order* reused = pool.allocate();
    ASSERT_NE(reused, nullptr);
    EXPECT_EQ(reused, first); 

    EXPECT_EQ(reused->id, 0u);
}

TEST_F(ObjectPoolTest, CapacityOverflowReturnsNull) {
    for (size_t i = 0; i < POOL_CAPACITY; i++) {
        Order* o = pool.allocate();
        ASSERT_NE(o, nullptr) << "Failed at allocation " << i;
    }

    Order* overflow = pool.allocate();
    EXPECT_EQ(overflow, nullptr);
}

TEST_F(ObjectPoolTest, DeallocateRestoresCapacity) {
    std::vector<Order*> ptrs;
    for (size_t i = 0; i < POOL_CAPACITY; i++) {
        ptrs.push_back(pool.allocate());
    }

    EXPECT_EQ(pool.allocate(), nullptr);

    pool.deallocate(ptrs.back());
    ptrs.pop_back();

    Order* o = pool.allocate();
    EXPECT_NE(o, nullptr);

    EXPECT_EQ(pool.allocate(), nullptr);
}

TEST_F(ObjectPoolTest, AllocateDeallocateCycle) {
    for (int cycle = 0; cycle < 1000; cycle++) {
        Order* o = pool.allocate();
        ASSERT_NE(o, nullptr) << "Failed at cycle " << cycle;
        o->id = cycle;
        o->price = 15000 + cycle;
        pool.deallocate(o);
    }
    Order* final_alloc = pool.allocate();
    EXPECT_NE(final_alloc, nullptr);
}

TEST_F(ObjectPoolTest, FreeListOrderIsLIFO) {
    Order* a = pool.allocate();
    Order* b = pool.allocate();
    Order* c = pool.allocate();

    pool.deallocate(a);
    pool.deallocate(b);
    pool.deallocate(c);

    EXPECT_EQ(pool.allocate(), c);
    EXPECT_EQ(pool.allocate(), b);
    EXPECT_EQ(pool.allocate(), a);
}
