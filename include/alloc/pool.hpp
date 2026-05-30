#pragma once

#include <cstddef>
#include <cstring>
#include <new>

namespace alloc
{
    class PoolAllocator
    {
    public:
        PoolAllocator(std::size_t req_size,
                      std::size_t req_align,
                      std::size_t num_blocks)
            : num_blocks_(num_blocks)
        {
            // Repair 1: a block must be big enough to hold a next-pointer.
            block_size_ = req_size < sizeof(void *) ? sizeof(void *) : req_size;

            // Repair 2: a block must be aligned enough to hold a pointer.
            block_align_ = req_align < alignof(void *) ? alignof(void *) : req_align;

            // Round block_size_ up to a multiple of block_align_
            block_size_ = (block_size_ + block_align_ - 1) & ~(block_align_ - 1);

            if (num_blocks == 0)
            {
                return;
            }

            const std::size_t bytes = block_size_ * num_blocks_;
            buffer_ = static_cast<std::byte *>(::operator new(bytes, std::align_val_t{block_align_}));

            for (std::size_t i = 0; i < num_blocks; i++)
            {
                std::byte *block = buffer_ + i * block_size_;
                void *next = (i + 1 < num_blocks_) ? static_cast<void *>(buffer_ + (i + 1) * block_size_) : nullptr;
                std::memcpy(block, &next, sizeof(next));
            }

            free_head_ = buffer_;
            free_count_ = num_blocks_;
        }

        ~PoolAllocator()
        {
            ::operator delete(buffer_, std::align_val_t{block_align_});
        }

        PoolAllocator(const PoolAllocator &) = delete;
        PoolAllocator &operator=(const PoolAllocator &) = delete;

        PoolAllocator(PoolAllocator &&other) noexcept
            : buffer_(other.buffer_),
              free_head_(other.free_head_),
              block_size_(other.block_size_),
              block_align_(other.block_align_),
              num_blocks_(other.num_blocks_),
              free_count_(other.free_count_)
        {
            other.buffer_ = nullptr;
            other.free_head_ = nullptr;
            other.num_blocks_ = 0;
            other.free_count_ = 0;
        }

        PoolAllocator &operator=(PoolAllocator &&other) noexcept
        {
            if (this != &other)
            {
                // Release our own buffer first, then steal theirs.
                ::operator delete(buffer_, std::align_val_t{block_align_});

                buffer_ = other.buffer_;
                free_head_ = other.free_head_;
                block_size_ = other.block_size_;
                block_align_ = other.block_align_;
                num_blocks_ = other.num_blocks_;
                free_count_ = other.free_count_;

                other.buffer_ = nullptr;
                other.free_head_ = nullptr;
                other.num_blocks_ = 0;
                other.free_count_ = 0;
            }
            return *this;
        }

        [[nodiscard]] void *alloc() noexcept
        {
            if (free_head_ == nullptr)
            {
                return nullptr;
            }

            void *block = free_head_;

            void *next;
            std::memcpy(&next, block, sizeof(next));
            free_head_ = next;

            --free_count_;
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
        }

        [[nodiscard]] std::size_t block_size() const noexcept { return block_size_; }
        [[nodiscard]] std::size_t block_align() const noexcept { return block_align_; }
        [[nodiscard]] std::size_t capacity() const noexcept { return num_blocks_; }
        [[nodiscard]] std::size_t available() const noexcept { return free_count_; }
        [[nodiscard]] std::size_t used() const noexcept { return num_blocks_ - free_count_; }

    private:
        std::byte *buffer_;
        void *free_head_ = nullptr;
        std::size_t block_size_;
        std::size_t block_align_;
        std::size_t num_blocks_;
        std::size_t free_count_;
    };
}
