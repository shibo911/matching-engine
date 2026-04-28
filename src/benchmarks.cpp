#include <benchmark/benchmark.h>
#include "core/MatchingEngine.hpp"
#include "core/NaiveEngine.hpp"
#include "memory/EngineMemory.hpp"
#include "data/Order.hpp"
#include <vector>

using namespace matching_engine;

// Generate simulated market data
std::vector<data::Order> generate_orders(size_t count) {
    std::vector<data::Order> orders;
    orders.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        data::Order o;
        o.id = i + 1;
        o.price = 15000 + (i % 100); // Ticks clustered around $150
        o.quantity = 100;
        o.side = (i % 2 == 0) ? data::Side::Buy : data::Side::Sell;
        orders.push_back(o);
    }
    return orders;
}

// ----------------------------------------------------------------------------
// BENCHMARK 1: 100,000 INSERTIONS
// ----------------------------------------------------------------------------
static void BM_Optimized_Insertion(benchmark::State& state) {
    memory::EngineMemory mem(1024 * 1024 * 500); // 500MB
    auto alloc = mem.get_allocator();
    data::LimitOrderBook lob(10000, 30000, alloc);
    data::CancelLookup cancel(state.range(0) * 2, alloc);
    memory::ObjectPool<data::Order> pool(state.range(0) * 2, alloc);

    auto orders = generate_orders(state.range(0));
    
    // We pre-allocate pointers so we strictly benchmark LOB logic, not ObjectPool overhead
    std::vector<data::Order*> ptrs(state.range(0));
    for (size_t i = 0; i < state.range(0); ++i) {
        ptrs[i] = pool.allocate();
        *ptrs[i] = orders[i];
    }

    for (auto _ : state) {
        for (size_t i = 0; i < state.range(0); ++i) {
            cancel.register_order(ptrs[i]);
            lob.add_order(ptrs[i]);
        }
        
        // Cleanup phase doesn't count against time
        state.PauseTiming();
        for (size_t i = 0; i < state.range(0); ++i) {
            lob.remove_order(ptrs[i]);
            cancel.deregister_order(ptrs[i]->id);
        }
        state.ResumeTiming();
    }
}

static void BM_Naive_Insertion(benchmark::State& state) {
    auto orders = generate_orders(state.range(0));

    for (auto _ : state) {
        core::NaiveEngine engine;
        for (size_t i = 0; i < state.range(0); ++i) {
            engine.add_order(orders[i]);
        }
    }
}

// ----------------------------------------------------------------------------
// BENCHMARK 2: 100,000 CANCELLATIONS
// ----------------------------------------------------------------------------
static void BM_Optimized_Cancellation(benchmark::State& state) {
    memory::EngineMemory mem(1024 * 1024 * 500); 
    auto alloc = mem.get_allocator();
    data::LimitOrderBook lob(10000, 30000, alloc);
    data::CancelLookup cancel(state.range(0) * 2, alloc);
    memory::ObjectPool<data::Order> pool(state.range(0) * 2, alloc);

    auto orders = generate_orders(state.range(0));
    std::vector<data::Order*> ptrs(state.range(0));
    
    for (auto _ : state) {
        state.PauseTiming();
        // Setup Book
        for (size_t i = 0; i < state.range(0); ++i) {
            ptrs[i] = pool.allocate();
            *ptrs[i] = orders[i];
            cancel.register_order(ptrs[i]);
            lob.add_order(ptrs[i]);
        }
        state.ResumeTiming();

        // O(1) Cancellation magic
        for (size_t i = 0; i < state.range(0); ++i) {
            lob.remove_order(ptrs[i]);
            cancel.deregister_order(ptrs[i]->id);
        }
        
        state.PauseTiming();
        for (size_t i = 0; i < state.range(0); ++i) {
            pool.deallocate(ptrs[i]);
        }
        state.ResumeTiming();
    }
}

static void BM_Naive_Cancellation(benchmark::State& state) {
    auto orders = generate_orders(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();
        core::NaiveEngine engine;
        for (size_t i = 0; i < state.range(0); ++i) {
            engine.add_order(orders[i]);
        }
        state.ResumeTiming();

        for (size_t i = 0; i < state.range(0); ++i) {
            engine.cancel_order(orders[i].id);
        }
    }
}

// ----------------------------------------------------------------------------
// BENCHMARK 3: 100,000 FULL MATCHES
// ----------------------------------------------------------------------------
static void BM_Optimized_Matching(benchmark::State& state) {
    memory::EngineMemory mem(1024 * 1024 * 500); 
    auto alloc = mem.get_allocator();
    
    data::LimitOrderBook lob(10000, 30000, alloc);
    data::CancelLookup cancel(state.range(0) * 2, alloc);
    memory::ObjectPool<data::Order> pool(state.range(0) * 2, alloc);
    concurrency::SPMCBroadcastQueue<data::TradeEvent, 1024> egress(alloc);
    core::MatchingEngine engine(&lob, &cancel, &pool, &egress);

    for (auto _ : state) {
        state.PauseTiming();

        // Pre-fill book with Makers (Sell)
        for (size_t i = 0; i < state.range(0); ++i) {
            data::Order* maker = pool.allocate();
            maker->id = i + 1;
            maker->price = 15000 + (i % 100);
            maker->quantity = 10;
            maker->side = data::Side::Sell;
            cancel.register_order(maker);
            lob.add_order(maker);
        }

        // Setup aggressive Takers (Buy) to sweep the book
        std::vector<data::Order*> takers(state.range(0));
        for (size_t i = 0; i < state.range(0); ++i) {
            takers[i] = pool.allocate();
            takers[i]->id = state.range(0) + i + 1;
            takers[i]->price = 20000; // Guaranteed to cross
            takers[i]->quantity = 10;
            takers[i]->side = data::Side::Buy;
        }
        state.ResumeTiming();

        // The Ultimate Test: O(1) matching vs 100,000 orders
        for (size_t i = 0; i < state.range(0); ++i) {
            engine.process_order(takers[i]);
        }
    }
}

static void BM_Naive_Matching(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        core::NaiveEngine engine;
        for (size_t i = 0; i < state.range(0); ++i) {
            data::Order maker;
            maker.id = i + 1;
            maker.price = 15000 + (i % 100);
            maker.quantity = 10;
            maker.side = data::Side::Sell;
            engine.add_order(maker);
        }

        std::vector<data::Order> takers(state.range(0));
        for (size_t i = 0; i < state.range(0); ++i) {
            data::Order taker;
            taker.id = state.range(0) + i + 1;
            taker.price = 20000; 
            taker.quantity = 10;
            taker.side = data::Side::Buy;
            takers[i] = taker;
        }
        state.ResumeTiming();

        for (size_t i = 0; i < state.range(0); ++i) {
            engine.match_order(takers[i]);
        }
    }
}

// Arg(100000) simulates 100k events
BENCHMARK(BM_Optimized_Insertion)->Arg(100000)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Naive_Insertion)->Arg(100000)->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Optimized_Cancellation)->Arg(100000)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Naive_Cancellation)->Arg(100000)->Unit(benchmark::kMillisecond);

BENCHMARK(BM_Optimized_Matching)->Arg(100000)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_Naive_Matching)->Arg(100000)->Unit(benchmark::kMillisecond);
