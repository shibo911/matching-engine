#pragma once

#include <atomic>
#include <cstddef>
#include <vector>
#include <memory_resource>

namespace matching_engine {
namespace concurrency {

inline constexpr std::size_t SPMC_CACHE_LINE_SIZE = 64;

// SPMC Broadcast Queue using the Disruptor Seqlock pattern.
// Producer NEVER waits (Wait-Free). Multiple Consumers read independently.
// Perfect for broadcasting TradeEvents to N WebSocket UI threads.
template <typename T, std::size_t Capacity>
class SPMCBroadcastQueue {
    static_assert((Capacity != 0) && ((Capacity & (Capacity - 1)) == 0), 
                  "Capacity must be a power of 2 for bitwise optimization");

    // A memory slot guarded by an atomic sequence lock
    struct Slot {
        std::atomic<std::size_t> sequence{0};
        T data;
    };

public:
    SPMCBroadcastQueue(std::pmr::polymorphic_allocator<std::byte> alloc)
        : buffer_(Capacity, alloc) 
    {
        head_sequence_.store(0, std::memory_order_relaxed);
    }

    SPMCBroadcastQueue(const SPMCBroadcastQueue&) = delete;
    SPMCBroadcastQueue& operator=(const SPMCBroadcastQueue&) = delete;

    // Called EXCLUSIVELY by the PRODUCER (Pinned Engine Thread)
    // O(1) Absolute Wait-Free execution. 
    inline void push(const T& item) {
        const size_t seq = head_sequence_.load(std::memory_order_relaxed);
        const size_t index = (seq / 2) & (Capacity - 1);
        
        // 1. Mark slot as "writing" (Odd sequence number)
        buffer_[index].sequence.store(seq + 1, std::memory_order_release);
        
        // 2. Perform non-atomic data write (Fastest path)
        buffer_[index].data = item;
        
        // 3. Mark slot as "finished" (Even sequence number)
        buffer_[index].sequence.store(seq + 2, std::memory_order_release);
        
        // 4. Advance global head
        head_sequence_.store(seq + 2, std::memory_order_release);
    }

    // Called by MULTIPLE CONSUMERS concurrently (e.g., WebSocket Gateway Threads)
    // Consumers pass their own local_read_sequence pointer.
    inline bool pop(std::size_t& local_read_sequence, T& out_item) {
        const size_t current_head = head_sequence_.load(std::memory_order_acquire);
        
        // If consumer is fully caught up, nothing to read.
        if (local_read_sequence >= current_head) [[likely]] {
            return false;
        }

        const size_t index = (local_read_sequence / 2) & (Capacity - 1);
        
        // SEQLOCK READ ALGORITHM
        size_t seq_start;
        do {
            seq_start = buffer_[index].sequence.load(std::memory_order_acquire);
            
            // If seq_start is odd, Producer is currently mid-write.
            // If seq_start != local+2, Producer has lapped us and overwritten the data!
            if ((seq_start & 1) != 0 || seq_start != local_read_sequence + 2) {
                 // The consumer is too slow. Drop frames and fast-forward to the present
                 // to prevent infinite spinning on dead data.
                 local_read_sequence = current_head; 
                 return false; 
            }

            // Perform non-atomic read into local copy
            out_item = buffer_[index].data;
            
            // Validate the sequence hasn't changed during our memory copy!
            // If it changed, the Producer overwrote it while we were reading (Data Tearing).
            // The do-while loop will catch this and retry/fail safely.
        } while (buffer_[index].sequence.load(std::memory_order_acquire) != seq_start);

        local_read_sequence += 2;
        return true;
    }

private:
    std::pmr::vector<Slot> buffer_;
    
    // Aligned to completely isolate the global head counter from buffer memory
    alignas(SPMC_CACHE_LINE_SIZE) std::atomic<std::size_t> head_sequence_{0};
};

} // namespace concurrency
} // namespace matching_engine
