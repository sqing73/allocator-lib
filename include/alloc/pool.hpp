#pragma once

#include <cstddef>
#include <cstring>
#include <new>
#include <alloc/bump.hpp>

namespace alloc
{
    class PoolAllocator
    {
    public:
        PoolAllocator(BumpAllocator &arena,
                      std::size_t req_size,
                      std::size_t req_align,
                      std::size_t blocks_per_slab) noexcept
            : arena_(arena),
              blocks_per_slab_(blocks_per_slab)
        {
            // Repair 1: a block must be big enough to hold a next-pointer.
            block_size_ = req_size < sizeof(void *) ? sizeof(void *) : req_size;

            // Repair 2: a block must be aligned enough to hold a pointer.
            block_align_ = req_align < alignof(void *) ? alignof(void *) : req_align;

            // Round block_size_ up to a multiple of block_align_
            block_size_ = (block_size_ + block_align_ - 1) & ~(block_align_ - 1);
        }

        ~PoolAllocator() = default;

        PoolAllocator(const PoolAllocator &) = delete;
        PoolAllocator &operator=(const PoolAllocator &) = delete;

        PoolAllocator(PoolAllocator &&other) = delete;

        PoolAllocator &operator=(PoolAllocator &&other) noexcept = delete;

        [[nodiscard]] void *alloc() noexcept
        {
            if (free_head_ == nullptr)
            {
                if (!grow())
                    return nullptr;
            }

            void *block = free_head_;

            void *next;
            std::memcpy(&next, block, sizeof(next));
            free_head_ = next;

            --free_count_;
            ++used_count_;
            return block;
        }

        void free(void *p) noexcept
        {
            if (p == nullptr)
            {
                return;
            }

            std::memcpy(p, &free_head_, sizeof(free_head_));
            free_head_ = p;

            ++free_count_;
            --used_count_;
        }

        [[nodiscard]] std::size_t block_size() const noexcept { return block_size_; }
        [[nodiscard]] std::size_t block_align() const noexcept { return block_align_; }
        [[nodiscard]] std::size_t available() const noexcept { return free_count_; }
        [[nodiscard]] std::size_t used() const noexcept { return used_count_; }

        [[nodiscard]] std::size_t total_blocks() const noexcept
        {
            return free_count_ + used_count_;
        }

    private:
        bool grow() noexcept
        {
            const std::size_t bytes = block_size_ * blocks_per_slab_;
            void *slab = arena_.alloc(bytes, block_align_);
            if (slab == nullptr)
            {
                return false;
            }
            std::byte *base = static_cast<std::byte *>(slab);
            for (std::size_t i = 0; i < blocks_per_slab_; i++)
            {
                std::byte *block = base + i * block_size_;
                void *next = (i + 1 < blocks_per_slab_)
                                 ? static_cast<void *>(block + block_size_)
                                 : free_head_;
                std::memcpy(block, &next, sizeof(next));
            }

            free_head_ = slab;
            free_count_ += blocks_per_slab_;
            return true;
        }

        BumpAllocator &arena_;
        void *free_head_ = nullptr;
        std::size_t block_size_;
        std::size_t block_align_;
        std::size_t blocks_per_slab_;
        std::size_t free_count_ = 0;
        std::size_t used_count_ = 0;
    };
}
