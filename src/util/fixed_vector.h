#pragma once

#include <stdexcept>
#include <iterator>
#include <memory>

namespace intersections::util {

template<class T, class Allocator = std::allocator<T>>
class fixed_vector
{
public:
    using value_type = T;
    using allocator_type = Allocator;
    using pointer = value_type*;
    using const_pointer = value_type const*;
    using reference = value_type&;
    using const_reference = value_type const&;

    fixed_vector()
            : fixed_vector(0)
    {}

    explicit fixed_vector(size_t size,
                          const allocator_type& allocator = allocator_type())
            : allocator_(allocator), size_(size),
              data_(allocate_(allocator_, size_)) {}

    explicit fixed_vector(size_t size,
                          const_reference element,
                          const allocator_type& allocator = allocator_type())
            : allocator_(allocator), size_(size),
              data_(allocate_(allocator_, size_, element)) {}

    ~fixed_vector()
    {
        deallocate_();
    }

    fixed_vector(fixed_vector&& other) : fixed_vector()
    {
        swap(other);
    }

    fixed_vector& operator=(fixed_vector&& other)
    {
        swap(other);
        other.clear();
    }

    fixed_vector(const fixed_vector&) = delete;
    fixed_vector& operator=(const fixed_vector&) = delete;

    bool empty() const
    {
        return size_ == 0;
    }

    size_t size() const
    {
        return size_;
    }

    void swap(fixed_vector& other)
    {
        using std::swap;
        swap(allocator_, other.allocator_);
        swap(size_, other.size_);
        swap(data_, other.data_);
    }

    void clear()
    {
        if (size_ != 0) {
            deallocate_();
            data_ = allocate_(allocator_, 0);
        }
    }

    const_reference operator[](size_t index) const
    {
        return data_[index];
    }

    reference operator[](size_t index)
    {
        return data_[index];
    }

    const_reference at(size_t index) const
    {
        if (index >= size_) throw std::range_error("fixed_vector");
        return data_[index];
    }

    reference at(size_t index)
    {
        if (index >= size_) throw std::range_error("fixed_vector");
        return data_[index];
    }

    using iterator = pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    iterator begin() { return data_; }

    const_iterator begin() const { return data_; }

    const_iterator cbegin() const { return data_; }

    iterator end() { return data_ + size_; }

    const_iterator end() const { return data_ + size_; }

    const_iterator cend() const { return data_ + size_; }

    reverse_iterator rbegin() { return {end()}; }

    const_reverse_iterator rbegin() const { return {end()}; }

    const_reverse_iterator crbegin() const { return {end()}; }

    reverse_iterator rend() { return {begin()}; }

    const_reverse_iterator rend() const { return {begin()}; }

    const_reverse_iterator crend() const { return {begin()}; }

private:
    allocator_type allocator_;
    size_t size_;
    T* data_;

    using allocator_trait = std::allocator_traits<allocator_type>;

    template<class... Args>
    static T* allocate_(allocator_type& allocator,
                        size_t size,
                        Args... args)
    {
        auto new_data = allocator_trait::allocate(allocator, size);
        for (size_t i = 0; i < size; ++i)
            new(&new_data[i]) T(std::forward<Args>(args)...);
        return new_data;
    }

    void deallocate_()
    {
        for (size_t i = 0; i < size_; ++i) data_[i].~T();
        allocator_trait::deallocate(allocator_, data_, size_);
    }
};

} // end namespace intersections::util