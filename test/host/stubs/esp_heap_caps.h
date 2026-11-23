#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>

constexpr unsigned MALLOC_CAP_DEFAULT = 0;
constexpr unsigned MALLOC_CAP_SPIRAM = 1;

namespace host_heap
{
inline bool fail_next = false;
inline int live_allocations = 0;
}

inline void *heap_caps_malloc(std::size_t size, unsigned)
{
    if (host_heap::fail_next)
    {
        host_heap::fail_next = false;
        return nullptr;
    }
    void *ptr = std::malloc(size);
    if (ptr != nullptr)
        ++host_heap::live_allocations;
    return ptr;
}

inline void heap_caps_free(void *ptr)
{
    if (ptr != nullptr)
        --host_heap::live_allocations;
    std::free(ptr);
}
