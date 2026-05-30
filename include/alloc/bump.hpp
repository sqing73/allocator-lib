#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <new>

namespace alloc
{
    class BumpAllocator
    {
    public:
        BumpAllocator(void *buffer, std::size_t size) noexcept
            : buffer_(static_cast<std::byte *>(buffer)), size_(size), offset_(0)
        {
            assert(buffer != nullptr && size != 0);
        }
        BumpAllocator(const BumpAllocator &) = delete;
        BumpAllocator &operator=(const BumpAllocator &) = delete;
        BumpAllocator(BumpAllocator &&) = delete;
        BumpAllocator &operator=(BumpAllocator &&) = delete;

        [[nodiscard]] void *alloc(std::size_t size, std::size_t align) noexcept
        {
            assert(align != 0 && (align & (align - 1)) == 0);

            auto current = reinterpret_cast<std::uintptr_t>(buffer_ + offset_);
            auto aligned = (current + align - 1) & ~(align - 1);
            std::size_t padding = aligned - current;

            if (offset_ + padding + size > size_)
            {
                return nullptr;
            }

            offset_ += padding + size;
            return buffer_ + (offset_ - size);
        }

        void reset() noexcept
        {
            offset_ = 0;
        }

        [[nodiscard]] std::size_t used() const noexcept
        {
            return offset_;
        }

        [[nodiscard]] std::size_t capacity() const noexcept
        {
            return size_;
        }

        [[nodiscard]] std::size_t remaining() const noexcept
        {
            return size_ - offset_;
        }

    private:
        std::byte *buffer_;
        std::size_t size_;
        std::size_t offset_;
    };
}
