#pragma once

#include <memory_resource>
#include <vector>
#include <cstddef>

namespace matching_engine {
namespace memory {

class EngineMemory {
public:
    // Defaults to 500MB (500 * 1024 * 1024 bytes). 
    // This must cover the total size of the Object Pool, Cancel Map, and LOB array.
    explicit EngineMemory(std::size_t size_bytes = 524288000);

    // Disable copy/move to prevent accidental invalidation of the underlying PMR resource.
    EngineMemory(const EngineMemory&) = delete;
    EngineMemory& operator=(const EngineMemory&) = delete;
    EngineMemory(EngineMemory&&) = delete;
    EngineMemory& operator=(EngineMemory&&) = delete;

    // Passed to all internal data structures that need memory during initialization.
    std::pmr::polymorphic_allocator<std::byte> get_allocator();
    std::pmr::memory_resource* get_resource();

private:
    std::vector<std::byte> m_buffer;
    std::pmr::monotonic_buffer_resource m_resource;
};

} // namespace memory
} // namespace matching_engine
