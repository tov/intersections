#pragma once

#include "fixed_vector.h"

#include <cassert>
#include <climits>
#include <iterator>
#include <limits>
#include <memory>

namespace intersections::util {

static size_t const default_bucket_count = 8;
static double const grow_at_ratio = 0.75;

template <class T>
struct weak_traits
{
    using strong_type = typename T::strong_type;
    using key_type = typename T::key_type;

    static key_type* get_key(const strong_type& strong)
    {
        return strong_type::key(strong);
    }
};

template <class T>
struct weak_traits<std::weak_ptr<const T>>
{
    using strong_type = std::shared_ptr<const T>;
    using key_type = const T;

    static key_type* get_key(const strong_type& strong)
    {
        return strong? strong.get() : nullptr;
    }
};

template <class Key, class Value,
          class KeyWeakPtr = std::weak_ptr<const Key>,
          class ValueWeakPtr = std::weak_ptr<Value>,
          class KeyPtr = typename weak_traits<KeyWeakPtr>::strong_type,
          class ValuePtr = typename weak_traits<ValueWeakPtr>::strong_type>
struct weak_pair
{
    using key_type = Key;
    using value_type = Value;
    using key_pointer = KeyPtr;
    using value_pointer = ValuePtr;
    using key_weak_pointer = KeyWeakPtr;
    using value_weak_pointer = ValueWeakPtr;
    using strong_type = std::pair<key_pointer, value_pointer>;

    key_weak_pointer first;
    value_weak_pointer second;

    weak_pair(const strong_type& strong)
            : first(strong.first), second(strong.second)
    { }

    bool expired() const
    {
        return first.expired() || second.expired();
    }

    strong_type lock() const
    {
        if (auto key_ptr = first.lock())
            if (auto value_ptr = second.lock())
                return {key_ptr, value_ptr};

        return {nullptr, nullptr};
    }

    static const key_type* key(const strong_type& strong)
    {
        if (strong.first)
            return strong.first.get();
        else
            return nullptr;
    }
};

template <class Key, class Value,
          class KeyPtr = std::shared_ptr<const Key>,
          class KeyWeakPtr = typename KeyPtr::weak_type>
struct weak_key_pair
{
    using key_type = Key;
    using value_type = Value;
    using key_pointer = KeyPtr;
    using key_weak_pointer = KeyWeakPtr;
    using strong_type = std::pair<key_pointer, value_type&>;

    key_weak_pointer first;
    value_type second;

    weak_key_pair(const strong_type& strong)
            : first(strong.first), second(strong.second)
    { }

    bool expired() const
    {
        return first.expired();
    }

    strong_type lock() const
    {
        return {first.lock(), second};
    }

    static const key_type* key(const strong_type& strong)
    {
        if (strong.first)
            return strong.first.get();
        else
            return nullptr;
    }
};

template <class Key, class Value,
          class ValuePtr = std::shared_ptr<Value>,
          class ValueWeakPtr = typename ValuePtr::weak_type>
struct weak_value_pair
{
    using key_type = Key;
    using value_type = Value;
    using value_pointer = ValuePtr;
    using value_weak_pointer = ValueWeakPtr;
    using strong_type = std::pair<const key_type&, value_pointer>;

    key_type first;
    value_weak_pointer second;

    weak_value_pair(const strong_type& strong)
            : first(strong.first), second(strong.second)
    { }

    bool expired() const
    {
        return second.expired();
    }

    strong_type lock() const
    {
        return {first, second.lock()};
    }

    static const key_type* key(const strong_type& strong)
    {
        if (strong.second)
            return &strong.first;
        else
            return nullptr;
    }
};

/// A weak Robin Hood hash table storing std::weak_ptrs.
template <
    class T,
    class Hash = std::hash<typename weak_traits<T>::key_type>,
    class KeyEqual = std::equal_to<typename weak_traits<T>::key_type>,
    class Allocator = std::allocator<T>
>
class rh_weak_hash_table
{
public:
    using weak_value_type = T;
    using strong_value_type = typename weak_traits<T>::strong_type;
    using key_type = typename weak_traits<T>::key_type;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;

private:
    // We're going to steal a bit from the hash codes to store a used bit..
    // So the number of hash bits is one less than the number of bits in size_t.
    static constexpr size_t number_of_hash_bits_ =
        sizeof(size_t) * CHAR_BIT - 1;

    static constexpr size_t hash_code_mask_ =
        (size_t(1) << number_of_hash_bits_) - 1;

    // This class lifts the client-provided hasher to a hasher that masks out
    // the high bit.
    class real_hasher
    {
    public:
        explicit real_hasher(const Hash& hash) : hash_(hash) { }

        size_t operator()(const key_type& key) const
        {
            return hash_(key) & hash_code_mask_;
        }

    private:
        Hash hash_;
    };

    // We store the weak pointers in buckets along with a used bit and the
    // hash code for each bucket. hash_code_ is only valid if ptr_ is non-null.
    class Bucket
    {
    public:
        Bucket()
                : used_(0), hash_code_(0)
        { }

        bool occupied() const
        {
            return used_ && !value_.expired();
        }

        strong_value_type lock() const
        {
            return value_.lock();
        }

    private:
        weak_value_type value_;
        size_t          used_ : 1,
                        hash_code_ : number_of_hash_bits_;

        friend class rh_weak_hash_table;
    };

public:
    using bucket_allocator_type =
        typename std::allocator_traits<allocator_type>
                     ::template rebind_alloc<Bucket>;

private:
    using vector_t = fixed_vector<Bucket, bucket_allocator_type>;

public:

    rh_weak_hash_table()
            : rh_weak_hash_table(default_bucket_count)
    { }

    explicit rh_weak_hash_table(
        size_t capacity,
        const hasher& hash = hasher(),
        const key_equal& equal = key_equal(),
        const allocator_type& allocator = allocator_type())
            : hash_(real_hasher(hash))
            , equal_(equal)
            , bucket_allocator_(allocator)
            , buckets_(capacity, bucket_allocator_)
            , size_(0)
    { }

    bool empty() const
    {
        return size_ == 0;
    }

    size_t size() const
    {
        return size_;
    }

    void insert(const ptr_type& ptr)
    {
        insert_(hash_(*ptr), ptr);
        maybe_grow_();
    }

    void insert(ptr_type&& ptr)
    {
        size_t hash_code = hash_(*ptr);
        insert_(hash_code, std::move(ptr));
        maybe_grow_();
    }

    bool member(const value_type& key) const
    {
        return lookup_(key) != nullptr;
    }

    class iterator;
    using const_iterator = iterator;

    iterator begin() const
    {
        return {buckets_.begin(), buckets_.end()};
    }

    iterator end() const
    {
        return {buckets_.end(), buckets_.end()};
    }

    const_iterator cbegin() const
    {
        return begin();
    }

    const_iterator cend() const
    {
        return end();
    }

private:
    real_hasher hash_;
    key_equal equal_;
    bucket_allocator_type bucket_allocator_;

    vector_t buckets_;
    size_t size_;

    void maybe_grow_()
    {
        auto cap = buckets_.size();
        if (double(size_)/double(cap) > grow_at_ratio) {
            resize_(2 * cap);
        }
    }

    void resize_(size_t new_capacity)
    {
        assert(new_capacity > size_);

        using std::swap;
        vector_t old_buckets(new_capacity);
        swap(old_buckets, buckets_);

        size_ = 0;

        for (const Bucket& bucket : old_buckets) {
            if (bucket.used_) {
                if (auto ptr = bucket.ptr_.lock()) {
                    insert_(bucket.hash_code_, ptr);
                }
            }
        }
    }

    const Bucket* lookup_(const value_type& key) const
    {
        size_t hash_code = hash_(key);
        size_t pos = which_bucket_(hash_code);
        size_t dist = 0;

        for (;;) {
            const Bucket& bucket = buckets_[pos];
            if (!bucket.used_)
                return nullptr;

            if (dist > probe_distance_(pos, which_bucket_(bucket.hash_code_)))
                return nullptr;

            if (hash_code == bucket.hash_code_)
                if (auto locked = bucket.ptr_.lock())
                    if (equal_(*locked, key))
                        return &bucket;

            pos = next_bucket_(pos);
            ++dist;
        }
    }

    Bucket* lookup_(const value_type& key)
    {
        size_t hash_code = hash_(key);
        size_t pos = which_bucket_(hash_code);
        size_t dist = 0;

        for (;;) {
            Bucket& bucket = buckets_[pos];
            if (!bucket.used_)
                return nullptr;

            if (dist > probe_distance_(pos, which_bucket_(bucket.hash_code_)))
                return nullptr;

            if (hash_code == bucket.hash_code_)
                if (auto locked = bucket.ptr_.lock())
                    if (equal_(*locked, key))
                        return &bucket;

            pos = next_bucket_(pos);
            ++dist;
        }
    }

    // Based on https://www.sebastiansylvan.com/post/robin-hood-hashing-should-be-your-default-hash-table-implementation/
    bool insert_(size_t hash_code, ptr_type ptr)
    {
        size_t pos = which_bucket_(hash_code);
        size_t dist = 0;

        bool original_pointer = true;
        bool saved_original_pointer = false;

        for (;;) {
            Bucket& bucket = buckets_[pos];

            // If the bucket is unoccupied, use it:
            if (!bucket.used_) {
                bucket.ptr_ = ptr;
                bucket.hash_code_ = hash_code;
                bucket.used_ = 1;
                ++size_;
                return original_pointer || saved_original_pointer;
            }

            // Check if the pointer is expired. If it is, use this slot.
            auto locked = bucket.ptr_.lock();
            if (!locked) {
                bucket.ptr_ = ptr;
                bucket.hash_code_ = hash_code;
                return original_pointer || saved_original_pointer;
            }

            // If not expired, but matches the value to insert, replace.
            if (hash_code == bucket.hash_code_ && equal_(*locked, *ptr)) {
                bucket.ptr_ = ptr;
                return saved_original_pointer;
            }

            // Otherwise, we check the probe distance.
            size_t existing_distance =
                probe_distance_(pos, which_bucket_(bucket.hash_code_));
            if (dist > existing_distance) {
                saved_original_pointer = true;
                original_pointer = false;
                bucket.ptr_ = std::exchange(ptr, std::move(locked));
                size_t tmp = bucket.hash_code_;
                bucket.hash_code_ = hash_code;
                hash_code = tmp;
                dist = existing_distance;
            }

            pos = next_bucket_(pos);
            ++dist;
        }
    }

    size_t next_bucket_(size_t pos) const
    {
        return (pos + 1) % buckets_.size();
    }

    size_t probe_distance_(size_t actual, size_t preferred) const
    {
        if (actual >= preferred)
            return actual - preferred;
        else
            return actual + buckets_.size() - preferred;
    }

    size_t which_bucket_(size_t hash_code) const
    {
        return hash_code % buckets_.size();
    }

    size_t hash_value_(const value_type& value) const
    {
        return hash_(value) & hash_code_mask_;
    }
};

template <
    class Key,
    class Hash,
    class KeyEqual,
    class Allocator
>
class rh_weak_hash_table<Key, Hash, KeyEqual, Allocator>::iterator
        : public std::iterator<std::forward_iterator_tag, ptr_type>
{
public:
    using base_t = typename vector_t::const_iterator;

    iterator(base_t start, base_t limit)
            : base_(start), limit_(limit)
    {
        find_next_();
    }

    ptr_type operator*() const
    {
        return base_->ptr();
    }

    iterator& operator++()
    {
        ++base_;
        find_next_();
        return *this;
    }

    iterator operator++(int)
    {
        auto old = *this;
        ++*this;
        return old;
    }

    bool operator==(const iterator& other)
    {
        return base_ == other.base_;
    }

    bool operator!=(const iterator& other)
    {
        return base_ != other.base_;
    }

private:
    // Invariant: if base_ != limit_ then base_->occupied();
    base_t base_;
    base_t limit_;

    void find_next_()
    {
        while (base_ != limit_ && !base_->occupied())
            ++base_;
    }
};

} // end namespace intersections::util
