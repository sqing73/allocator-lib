#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <new>

namespace alloc
{
    template <typename T>
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

        Vector() noexcept
            : data_(nullptr), size_(0), capacity_(0) {}

        ~Vector()
        {
            destroy_all();
            deallocate(data_, capacity_);
        }

        Vector(const Vector &other)
        {
            if (other.size_ == 0)
                return;
            T *new_data = static_cast<T *>(::operator new(other.size_ * sizeof(T)));
            size_type built = 0;
            try
            {
                for (; built < other.size_; built++)
                {
                    ::new (new_data + built) T(other.data_[built]);
                }
            }
            catch (...)
            {
                for (size_type i = built; built > 0; --i)
                {
                    new_data[i].~T();
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

        void reserve(size_type n)
        {
            if (n <= capacity_)
                return;
            reallocate(n);
        }

        void push_back(T &val)
        {
            ensure_capacity();
            ::new (static_cast<void *>(data_ + size_)) T(val);
            ++size_;
        }

        void push_back(T &&val)
        {
            ensure_capacity();
            ::new (static_cast<void *>(data_ + size_)) T(std::move(val));
            ++size_;
        }

        template <typename... Args>
        T &emplace_back(Args &&... args)
        {
            if (size_ == capacity_)
                reallocate(capacity_ == 0 ? 1 : capacity_ * 2);
            pointer p = ::new (data_ + size_) T(std::forward<Args>(args)...);
            ++size_;
            return *p;
        }

        void clear()
        {
            destroy_all();
            size_ = 0;
        }

    private:
        static pointer allocate(size_type n)
        {
            if (n == 0)
                return nullptr;
            return static_cast<pointer>(::operator new(n * sizeof(T)));
        }

        void destroy_all() noexcept
        {
            for (size_type i = size_; i > 0; i--)
            {
                data_[i - 1].~T();
            }
        }

        void deallocate(pointer p, size_type /*n*/) noexcept
        {
            ::operator delete(p);
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
                        ::new (static_cast<void *>(new_data + migrated)) T(std::move(data_[migrated]));
                    }
                }
                else
                {
                    // use copy
                    for (; migrated < size_; ++migrated)
                    {
                        ::new (static_cast<void *>(new_data + migrated)) T(data_[migrated]);
                    }
                }
            }
            catch (...)
            {
                // Destroy already migrated
                for (size_type i = migrated; i > 0; --i)
                {
                    new_data[i].~T();
                }
                deallocate(new_data, new_cap);
                throw;
            }

            destroy_all();
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

        T *data_;
        size_type size_;
        size_type capacity_;
    };
}
