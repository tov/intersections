#pragma once

#include "fixed_vector.h"

#include <climits>
#include <iterator>
#include <limits>
#include <memory>

namespace intersections::util {

static size_t const default_bucket_count = 8;

namespace detail {

} // end namespace detail

template <
    class Key,
    class Hash = std::hash<Key>,
    class KeyEqual = std::equal_to<Key>,
    class Allocator = std::allocator<Key>
>
class rh_weak_unordered_set
{
public:
    using value_type = Key;
    using ptr_type = std::shared_ptr<const value_type>;
    using weak_ptr_type = std::weak_ptr<const value_type>;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;

private:
    // We're going to steal a bit from the hash codes to store a tombstone.
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

        size_t operator()(const value_type& value) const
        {
            return hash_(value) & hash_code_mask_;
        }

    private:
        Hash hash_;
    };

    // We store the weak pointers in buckets along with a tombstone bit and the
    // hash code for each bucket. tombstone_ and hash_code_ are only valid if
    // ptr_ is non-null.
    class Bucket
    {
        weak_ptr_type ptr_;
        size_t        tombstone_ : 1,
                      hash_code_ : number_of_hash_bits_;

        friend class rh_weak_unordered_set;

    public:
        bool occupied() const
        {
            return ptr_ && !tombstone_ && ptr_.lock();
        }

        ptr_type ptr() const
        {
            return {ptr_};
        }
    };

public:
    using bucket_allocator_type =
        typename std::allocator_traits<allocator_type>::template rebind_alloc<Bucket>;

private:
    using vector_t = fixed_vector<Bucket, bucket_allocator_type>;

public:

    rh_weak_unordered_set()
            : rh_weak_unordered_set(default_bucket_count)
    { }

    explicit rh_weak_unordered_set(
        size_t capacity,
        const hasher& hash = hasher(),
        const key_equal& equal = key_equal(),
        const allocator_type& allocator = allocator_type(),
        const bucket_allocator_type& bucket_allocator = bucket_allocator_type())
            : hash_(real_hasher(hash))
            , equal_(equal)
            , bucket_allocator_(bucket_allocator)
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

private:
    real_hasher hash_;
    key_equal equal_;
    bucket_allocator_type bucket_allocator_;

    vector_t buckets_;
    size_t size_;

    Bucket* lookup_(const value_type& key)
    {
        size_t hash_code = hash_(key);
        size_t pos = which_bucket_(hash_code);
        size_t dist = 0;

        for (;;) {
            Bucket& bucket = buckets_[pos];
            if (!bucket.ptr_ || bucket.tombstone_) {
                return nullptr;
            }

            if (dist > probe_distance_(pos, which_bucket_(bucket.hash_code_))) {
                return nullptr;
            }

            if (hash_code == bucket.hash_code_) {
                if (auto locked = bucket.ptr_.lock()) {
                    if (equal_(*locked, key)) {
                        return &bucket;
                    }
                }
            }

            pos = (pos + 1) % buckets_.size();
            ++dist;
        }
    }

    // Based on https://www.sebastiansylvan.com/post/robin-hood-hashing-should-be-your-default-hash-table-implementation/
    void insert_(size_t hash_code, ptr_type ptr)
    {
        size_t pos = which_bucket_(hash_code);
        size_t dist = 0;

        for (;;) {
            Bucket& bucket = buckets_[pos];

            // If the bucket is unoccupied, use it:
            if (!bucket.ptr_ || bucket.tombstone_) {
                bucket.ptr_ = ptr;
                bucket.hash_code_ = hash_code;
                bucket.tombstone_ = 0;
                ++size_;
                return;
            }

            // Check if the pointer is expired. If it is, use this slot.
            auto locked = bucket.ptr_.lock();
            if (!locked) {
                bucket.ptr_ = ptr;
                bucket.hash_code_ = hash_code;
                return;
            }

            // If not expired, but matches the value to insert, replace.
            if (hash_code == bucket.hash_code_ && equal_(*locked, *ptr)) {
                bucket.ptr_ = ptr;
                return;
            }

            // Otherwise, we check the probe distance.
            size_t existing_distance =
                probe_distance_(pos, which_bucket_(bucket.hash_code_));
            if (dist > existing_distance) {
                bucket.ptr_ = std::exchange(ptr, std::move(locked));
                dist = existing_distance;
            }

            pos = (pos + 1) % buckets_.size();
            ++dist;
        }
    }

    size_t probe_distance_(size_t actual, size_t preferred)
    {
        if (actual >= preferred)
            return actual - preferred;
        else
            return actual + buckets_.size() - preferred;
    }

    size_t which_bucket_(size_t hash_code)
    {
        return hash_code % buckets_.size();
    }

    size_t hash_value_(const value_type& value) const
    {
        return hash_(value) & hash_code_mask_;
    }
};

template <class Key, class Hash, class KeyEqual, class Allocator>
class rh_weak_unordered_set<Key, Hash, KeyEqual, Allocator>::iterator
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
