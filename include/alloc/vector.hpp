#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <new>

namespace alloc
{
    template <typename T, std::size_t N = 0, typename Allocator = std::allocator<T>>
    class Vector
    {
    public:
        using value_type = T;
        using size_type = std::size_t;
        using reference = T &;
        using const_reference = const T &;
        using pointer = T *;
        using const_pointer = const T *;
        using iterator = T *;
        using const_iterator = const T *;
        using traits = std::allocator_traits<Allocator>;
        using allocator_type = Allocator;

        static constexpr std::size_t kBufBytes = (N == 0) ? 1 : N * sizeof(T);

        Vector() noexcept(std::is_nothrow_default_constructible_v<Allocator>)
        {
            if constexpr (N == 0)
            {
                data_ = nullptr;
                capacity_ = 0;
            }
            else
            {
                data_ = inline_ptr();
                capacity_ = N;
            }
            size_ = 0;
        };

        explicit Vector(const Allocator &a) noexcept
            : data_(nullptr), size_(0), capacity_(0), alloc_(a) {}

        ~Vector()
        {
            destroy_all();
            if (is_heap())
                deallocate(data_, capacity_);
        }

        Vector(const Vector &other)
        {
            if (other.size_ == 0)
                return;
            T *new_data = allocate(other.size_);
            size_type built = 0;
            try
            {
                for (; built < other.size_; built++)
                {
                    traits::construct(alloc_, new_data + built, other.data_[built]);
                }
            }
            catch (...)
            {
                for (size_type i = built; i > 0; --i)
                {
                    traits::destroy(alloc_, new_data + (i - 1));
                }
                deallocate(new_data, other.size_);
                throw;
            }
            data_ = new_data;
            size_ = other.size_;
            capacity_ = other.size_;
        }

        Vector(Vector &&other) noexcept
            : data_(other.data_), size_(other.size_), capacity_(other.capacity_)
        {
            other.data_ = nullptr;
            other.size_ = 0;
            other.capacity_ = 0;
        }

        Vector &operator=(const Vector &other)
        {
            Vector tmp(other);
            swap(tmp);
            return *this;
        }

        Vector &operator=(Vector &&other) noexcept
        {
            if (this != &other)
            {
                clear();
                deallocate(data_, capacity_);
                data_ = other.data_;
                capacity_ = other.capacity_;
                size_ = other.size_;
                other.data_ = nullptr;
                other.size_ = 0;
                other.capacity_ = 0;
            }
            return *this;
        }

        // -----------------------------------------------------------------------
        // Element access
        // -----------------------------------------------------------------------

        reference operator[](size_type i) noexcept
        {
            return data_[i];
        }
        const_reference operator[](size_type i) const noexcept
        {
            return data_[i];
        }

        reference at(size_t i)
        {
            if (i >= size_)
                throw std::out_of_range("Vector::at");
            return data_[i];
        }

        pointer data() noexcept { return data_; }
        const_pointer data() const noexcept { return data_; }

        // -----------------------------------------------------------------------
        // Iterators
        // -----------------------------------------------------------------------

        iterator begin() noexcept { return data_; }
        const_iterator begin() const noexcept { return data_; }
        iterator end() noexcept { return data_ + size_; }
        const_iterator end() const noexcept { return data_ + size_; }

        // -----------------------------------------------------------------------
        // Capacity
        // -----------------------------------------------------------------------

        bool empty() const noexcept { return size_ == 0; }
        size_type size() const noexcept { return size_; }
        size_type capacity() const noexcept { return capacity_; }

        allocator_type get_allocator() const noexcept { return alloc_; }

        bool is_heap() const noexcept
        {
            if constexpr (N == 0)
                return data_ != nullptr;
            else
                return data_ != inline_ptr();
        }

        void reserve(size_type n)
        {
            if (n <= capacity_)
                return;
            reallocate(n);
        }

        void push_back(T &val)
        {
            ensure_capacity();
            traits::construct(alloc_, data_ + size_, val);
            ++size_;
        }

        void push_back(T &&val)
        {
            ensure_capacity();
            traits::construct(alloc_, data_ + size_, std::move(val));
            ++size_;
        }

        template <typename... Args>
        T &emplace_back(Args &&...args)
        {
            if (size_ == capacity_)
                reallocate(capacity_ == 0 ? 1 : capacity_ * 2);
            pointer p = data_ + size_;
            traits::construct(alloc_, data_ + size_, std::forward<Args>(args)...);

            ++size_;
            return *p;
        }

        void clear()
        {
            destroy_all();
            size_ = 0;
        }

    private:
        pointer allocate(size_type n)
        {
            if (n == 0)
                return nullptr;
            return static_cast<pointer>(traits::allocate(alloc_, n));
        }

        void destroy_all() noexcept
        {
            for (size_type i = size_; i > 0; i--)
            {
                traits::destroy(alloc_, data_ + (i - 1));
            }
        }

        void deallocate(pointer p, size_type n) noexcept
        {
            traits::deallocate(alloc_, p, n);
        }

        void ensure_capacity()
        {
            if (size_ == capacity_)
            {
                reallocate(capacity_ == 0 ? 1 : capacity_ * 2);
            }
        }

        void reallocate(size_type new_cap)
        {
            pointer new_data = allocate(new_cap);
            size_type migrated = 0;
            try
            {
                if constexpr (std::is_nothrow_move_constructible_v<T>)
                {
                    // use move
                    for (; migrated < size_; ++migrated)
                    {
                        traits::construct(alloc_, new_data + migrated, std::move(data_[migrated]));
                    }
                }
                else
                {
                    // use copy
                    for (; migrated < size_; ++migrated)
                    {
                        traits::construct(alloc_, new_data + migrated, data_[migrated]);
                    }
                }
            }
            catch (...)
            {
                // Destroy already migrated
                for (size_type i = migrated; i > 0; --i)
                {
                    traits::destroy(alloc_, new_data + (i - 1));
                }
                deallocate(new_data, new_cap);
                throw;
            }
            const bool old_was_heap = is_heap();
            destroy_all();
            if (old_was_heap)
                deallocate(data_, capacity_);

            data_ = new_data;
            capacity_ = new_cap;
        }

        void swap(Vector &other) noexcept
        {
            std::swap(data_, other.data_);
            std::swap(size_, other.size_);
            std::swap(capacity_, other.capacity_);
        }

        T *inline_ptr() noexcept { return reinterpret_cast<T *>(inline_buf_); }
        const T *inline_ptr() const noexcept { return reinterpret_cast<const T *>(inline_buf_); }

        T *data_;
        size_type size_;
        size_type capacity_;
        Allocator alloc_;
        alignas(T) std::byte inline_buf_[kBufBytes];
    };
}
