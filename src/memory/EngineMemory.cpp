#include "memory/EngineMemory.hpp"
#include <stdexcept>
#include <iostream>

namespace matching_engine {
namespace memory {

EngineMemory::EngineMemory(std::size_t size_bytes)
    : m_buffer(size_bytes),
      // We strictly pass std::pmr::null_memory_resource() as the upstream allocator.
      // If the 500MB buffer is exhausted, it throws std::bad_alloc instead of silently
      // allocating from the heap. This hard-enforces our zero-allocation rule.
      m_resource(m_buffer.data(), m_buffer.size(), std::pmr::null_memory_resource()) 
{
    // Eagerly fault in the physical pages.
    // If we just allocate the vector, the OS lazily assigns physical RAM. A page fault 
    // during the hot path would block the thread. Touching every 4KB page forces the OS 
    // to wire the memory upfront.
    for (std::size_t i = 0; i < m_buffer.size(); i += 4096) { 
        m_buffer[i] = std::byte{0};
    }
}

std::pmr::polymorphic_allocator<std::byte> EngineMemory::get_allocator() {
    return std::pmr::polymorphic_allocator<std::byte>(&m_resource);
}

std::pmr::memory_resource* EngineMemory::get_resource() {
    return &m_resource;
}

} // namespace memory
} // namespace matching_engine
