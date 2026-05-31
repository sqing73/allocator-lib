#pragma once

#include <alloc/pool.hpp>
#include <array>
#include <memory>

namespace alloc
{
    class SlabAllocator
    {
    public:
        SlabAllocator(std::size_t arena_bytes, std::size_t blocks_per_slab = 256)
            : buffer_(static_cast<std::byte *>(
                  ::operator new(arena_bytes, std::align_val_t{alignof(std::max_align_t)}))),
              buffer_size_(arena_bytes),
              arena_(buffer_, arena_bytes),
              pools_(make_pools(arena_, blocks_per_slab, std::make_index_sequence<kSizeClasses.size()>{}))
        {
        }

        ~SlabAllocator()
        {
            ::operator delete(buffer_, std::align_val_t{alignof(std::max_align_t)});
        }

        SlabAllocator(const SlabAllocator &) = delete;
        SlabAllocator &operator=(const SlabAllocator &) = delete;
        SlabAllocator(SlabAllocator &&) = delete;
        SlabAllocator &operator=(SlabAllocator &&) = delete;

        [[nodiscard]] void *alloc(std::size_t n) noexcept
        {
            const int idx = size_class_index(n);
            if (idx < 0)
            {
                return ::operator new(n, std::nothrow);
            }
            return pools_[idx].alloc();
        }

        void dealloc(void *p, std::size_t n) noexcept
        {
            if (p == nullptr)
            {
                return;
            }

            const int idx = size_class_index(n);
            if (idx < 0)
            {
                ::operator delete(p);
                return;
            }
            pools_[idx].free(p);
        }

        [[nodiscard]] static constexpr std::size_t num_size_classes() noexcept
        {
            return kSizeClasses.size();
        }

        [[nodiscard]] const PoolAllocator& pool(std::size_t n) noexcept {
            const int idx = size_class_index(n);
            return pools_[idx];
        }

    private:
        template <std::size_t... Is>
        static std::array<PoolAllocator, sizeof...(Is)>
        make_pools(BumpAllocator &arena,
                   std::size_t blocks_per_slab,
                   std::index_sequence<Is...>)
        {
            return std::array<PoolAllocator, sizeof...(Is)>{
                PoolAllocator(arena, kSizeClasses[Is], alignof(std::max_align_t), blocks_per_slab)...};
        }

        static constexpr int size_class_index(std::size_t n) noexcept
        {
            for (std::size_t i = 0; i < kSizeClasses.size(); i++)
            {
                if (n <= kSizeClasses[i])
                {
                    return static_cast<int>(i);
                }
            }
            return -1;
        }

        static constexpr std::array<std::size_t, 7> kSizeClasses = {
            8, 16, 32, 64, 128, 256, 512};
        std::size_t buffer_size_;
        std::byte *buffer_;
        alloc::BumpAllocator arena_;
        std::array<PoolAllocator, kSizeClasses.size()> pools_;
    };

}
