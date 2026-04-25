#pragma once
#include <vector>
#include <memory_resource>

namespace matching_engine {
namespace memory {

// A strictly pre-allocated Object Pool that uses an internal Free List
// to achieve O(1) lock-free allocation and deallocation without touching the heap.
template <typename T>
class ObjectPool {
public:
    ObjectPool(size_t capacity, std::pmr::polymorphic_allocator<std::byte> alloc)
        : pool_(alloc), free_list_(alloc) 
    {
        pool_.reserve(capacity); 
        free_list_.reserve(capacity); // Pre-allocate the free list to guarantee zero heap ops
    }

    template <typename... Args>
    T* allocate(Args&&... args) {
        // Fast path: Reuse a freed object
        if (!free_list_.empty()) [[likely]] {
            T* obj = free_list_.back();
            free_list_.pop_back();
            // Placement new to safely re-initialize the recycled memory block
            new (obj) T(std::forward<Args>(args)...);
            return obj;
        }

        // Slow path: Allocate a new object from the pool
        if (pool_.size() >= pool_.capacity()) [[unlikely]] {
            return nullptr; // Hard abort: Capacity exceeded
        }
        
        pool_.emplace_back(std::forward<Args>(args)...);
        return &pool_.back();
    }

    // O(1) Deallocation back to the Free List
    inline void deallocate(T* ptr) {
        free_list_.push_back(ptr);
    }

private:
    std::pmr::vector<T> pool_;
    std::pmr::vector<T*> free_list_;
};

} // namespace memory
} // namespace matching_engine
