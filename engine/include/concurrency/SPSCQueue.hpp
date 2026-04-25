#pragma once

#include <atomic>
#include <cstddef>
#include <vector>
#include <memory_resource>

namespace matching_engine {
namespace concurrency {

// x86 processors use 64-byte cache lines.
inline constexpr std::size_t CACHE_LINE_SIZE = 64;

// A wait-free Single-Producer Single-Consumer (SPSC) ring buffer.
template <typename T, std::size_t Capacity>
class SPSCQueue {
    // Mechanical Sympathy: 
    // Power-of-2 capacity allows us to completely eliminate expensive modulo 
    // division '%' on the CPU, replacing it with an ultra-fast bitwise AND '&' 
    // when wrapping array indices.
    static_assert((Capacity != 0) && ((Capacity & (Capacity - 1)) == 0), 
                  "SPSCQueue Capacity must be a power of 2");

public:
    SPSCQueue(std::pmr::polymorphic_allocator<std::byte> alloc)
        : buffer_(Capacity, alloc) 
    {
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    // Called exclusively by the PRODUCER thread (e.g., The Network Gateway)
    inline bool push(const T& item) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        
        // Memory Order Acquire: Ensures we see the latest tail updates from the Consumer
        const size_t current_tail = tail_.load(std::memory_order_acquire);

        // Check if ring buffer is full
        if (current_head - current_tail >= Capacity) [[unlikely]] {
            return false; 
        }

        // Bitwise AND for ultra-fast wrapping (mathematically equivalent to % Capacity)
        buffer_[current_head & (Capacity - 1)] = item;

        // Memory Order Release: Guarantee the buffer data write finishes BEFORE head is updated
        head_.store(current_head + 1, std::memory_order_release);
        return true;
    }

    // Called exclusively by the CONSUMER thread (e.g., The Pinned Matching Engine)
    inline bool pop(T& out_item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        
        // Memory Order Acquire: Ensures we see the latest head updates (and data) from the Producer
        const size_t current_head = head_.load(std::memory_order_acquire);

        // Check if ring buffer is empty
        if (current_head == current_tail) [[unlikely]] {
            return false; 
        }

        out_item = buffer_[current_tail & (Capacity - 1)];

        // Memory Order Release: Guarantee we read the data BEFORE telling Producer it's safe to overwrite
        tail_.store(current_tail + 1, std::memory_order_release);
        return true;
    }

private:
    std::pmr::vector<T> buffer_;

    // ---------------------------------------------------------
    // FALSE SHARING PREVENTION (The "Mechanical Sympathy" Core)
    // ---------------------------------------------------------
    // If head_ and tail_ shared the same 64-byte L1 cache line, the Producer Core
    // and Consumer Core would constantly invalidate each other's cache via the 
    // MESI protocol, creating a devastating latency bottleneck ("Cache Thrashing").
    // By forcing 64-byte alignment, they sit on physically distinct cache lines,
    // ensuring parallel threads never collide at the hardware level.
    
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> head_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<std::size_t> tail_{0};
};

} // namespace concurrency
} // namespace matching_engine
