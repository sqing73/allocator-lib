#pragma once
#include <cstdint>
#include <cstddef>

inline bool is_aligned(void *p, std::size_t align)
{
    return (reinterpret_cast<std::uintptr_t>(p) % align) == 0;
}
