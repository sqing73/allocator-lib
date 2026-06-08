#pragma once

#include <alloc/slab.hpp>
#include <cstddef>
#include <new>
#include <type_traits>

namespace alloc
{
    template <typename T>
    class STLSlabAllocator
    {
    public:
        using value_type = T;

        STLSlabAllocator() = delete;

        explicit STLSlabAllocator(SlabAllocator *s) noexcept : slab_(s) {}

        template <typename U>
        STLSlabAllocator(const STLSlabAllocator<U> &other) noexcept : slab_(other.slab_) {}

        T *allocate(std::size_t n)
        {
            if (n == 0)
                return nullptr;
            void *p = slab_->alloc(n * sizeof(T));
            if (!p)
                throw std::bad_alloc{};
            return static_cast<T *>(p);
        }

        void deallocate(T *p, std::size_t n) noexcept
        {
            if (p)
                slab_->dealloc(p, n * sizeof(T));
        }

        alloc::SlabAllocator *slab_;
    };

    template <typename T, typename U>
    bool operator==(const STLSlabAllocator<T> &a, const STLSlabAllocator<U> &b) noexcept
    {
        return a.slab_ == b.slab_;
    }

    template <typename T, typename U>
    bool operator!=(const STLSlabAllocator<T> &a, const STLSlabAllocator<U> &b) noexcept
    {
        return a.slab_ != b.slab_;
    }
}
