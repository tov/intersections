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
    /// strong_type guarantees the presence of a key.
    using strong_type = typename T::strong_type;
    /// view_type does not.
    using view_type = typename T::view_type;
    /// the type of keys
    using key_type = typename T::key_type;
    /// gets a pointer to a key from a view_type or a strong_type.
    using T::key;
    /// steals a view_type, turning it into a strong_type
    /// PRECONDITION: the view_type is not expired
    using T::move;
};

template <class T>
struct weak_traits<std::weak_ptr<T>>
{
    using strong_type = std::shared_ptr<T>;
    using view_type = strong_type;
    using key_type = T;

    static const view_type& view(const strong_type& strong)
    {
        return strong;
    }

    // This works for both view_type and strong_type, since they are the
    // same.
    static key_type* key(const view_type& view)
    {
        return view? view.get() : nullptr;
    }

    static strong_type move(view_type& view)
    {
        return std::move(view);
    }
};

template <class T>
struct weak_traits<std::weak_ptr<const T>>
{
    using strong_type = std::shared_ptr<const T>;
    using view_type = strong_type;
    using key_type = const T;

    static const view_type& view(const strong_type& strong)
    {
        return strong;
    }

    // This works for both view_type and strong_type, since they are the
    // same.
    static key_type* key(const view_type& view)
    {
        return view? view.get() : nullptr;
    }

    static strong_type move(view_type& view)
    {
        return std::move(view);
    }
};

template <class Key, class Value,
          class KeyWeakPtr = std::weak_ptr<const Key>,
          class ValueWeakPtr = std::weak_ptr<Value>>
struct weak_pair
{
    using key_type = Key;
    using value_type = Value;
    using key_pointer = typename weak_traits<KeyWeakPtr>::strong_type;
    using value_pointer = typename weak_traits<ValueWeakPtr>::strong_type;
    using key_weak_pointer = KeyWeakPtr;
    using value_weak_pointer = ValueWeakPtr;
    using strong_type = std::pair<key_pointer, value_pointer>;
    using view_type = strong_type;

    key_weak_pointer first;
    value_weak_pointer second;

    weak_pair(const strong_type& strong)
            : first(strong.first), second(strong.second)
    { }

    bool expired() const
    {
        return first.expired() || second.expired();
    }

    view_type lock() const
    {
        if (auto key_ptr = first.lock())
            if (auto value_ptr = second.lock())
                return {key_ptr, value_ptr};

        return {nullptr, nullptr};
    }

    static const key_type* key(const view_type& view)
    {
        if (view.first)
            return view.first.get();
        else
            return nullptr;
    }

    static strong_type move(view_type &view)
    {
        return std::move(view);
    }
};

template <class Key, class Value,
          class KeyWeakPtr = std::weak_ptr<const Key>>
struct weak_key_pair
{
    using key_type = Key;
    using value_type = Value;
    using key_pointer = typename weak_traits<KeyWeakPtr>::strong_type;
    using key_weak_pointer = KeyWeakPtr;
    using view_type = std::pair<key_pointer, value_type&>;
    using strong_type = std::pair<key_pointer, value_type>;

    key_weak_pointer first;
    value_type second;

    weak_key_pair(const strong_type& strong)
            : first(strong.first), second(strong.second)
    { }

    bool expired() const
    {
        return first.expired();
    }

    view_type lock() const
    {
        return {first.lock(), second};
    }

    static const key_type* key(const view_type& view)
    {
        if (view.first)
            return view.first.get();
        else
            return nullptr;
    }

    static const key_type& key(const strong_type& strong)
    {
        return strong.first.get();
    }

    static strong_type move(view_type& view)
    {
        return {std::move(view.first), std::move(view.second)};
    }
};

template <class Key, class Value,
          class ValueWeakPtr = std::weak_ptr<Value>>
struct weak_value_pair
{
    using key_type = Key;
    using value_type = Value;
    using value_pointer = typename weak_traits<ValueWeakPtr>::strong_type;
    using value_weak_pointer = ValueWeakPtr;
    using view_type = std::pair<const key_type&, value_pointer>;
    using strong_type = std::pair<key_type, value_pointer>;

    key_type first;
    value_weak_pointer second;

    weak_value_pair(const strong_type& strong)
            : first(strong.first), second(strong.second)
    { }

    bool expired() const
    {
        return second.expired();
    }

    view_type lock() const
    {
        return {first, second.lock()};
    }

    static const key_type* key(const view_type& view)
    {
        if (view.second)
            return &view.first;
        else
            return nullptr;
    }

    static const key_type* key(const strong_type& strong)
    {
        return &strong.first;
    }

    static strong_type move(view_type& view)
    {
        return {std::move(const_cast<key_type&>(view.first)),
                std::move(view.second)};
    }
};

/// A weak Robin Hood hash table.
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
    using weak_trait = weak_traits<weak_value_type>;
    using view_value_type = typename weak_trait::view_type;
    using strong_value_type = typename weak_trait::strong_type;
    using key_type = typename weak_trait::key_type;
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

        view_value_type lock() const
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

    // Note that because pointers may expire without the table finding
    // out, size() is generally an overapproximation of the number of
    // elements in the hash table.
    size_t size() const
    {
        return size_;
    }

    void insert(const strong_value_type& value)
    {
        insert_(hash_(*weak_trait::key(value)), value);
        maybe_grow_();
    }

    void insert(strong_value_type&& value)
    {
        size_t hash_code = hash_(*weak_trait::key(value));
        insert_(hash_code, std::move(value));
        maybe_grow_();
    }

    bool member(const key_type& key) const
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
                auto value = bucket.value_.lock();
                if (!bucket.value_.expired()) {
                    insert_(bucket.hash_code_, std::move(value));
                }
            }
        }
    }

    const Bucket* lookup_(const key_type& key) const
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

            if (hash_code == bucket.hash_code_) {
                auto locked = bucket.value_.lock();
                if (const auto* key = weak_trait::key(locked))
                    if (equal_(*locked, *key))
                        return &bucket;
            }

            pos = next_bucket_(pos);
            ++dist;
        }
    }

    Bucket* lookup_(const key_type& key)
    {
        auto const_this = const_cast<const rh_weak_hash_table*>(this);
        auto bucket = const_this->lookup_(key);
        return const_cast<Bucket*>(bucket);
    }

    // Based on https://www.sebastiansylvan.com/post/robin-hood-hashing-should-be-your-default-hash-table-implementation/
    void insert_(size_t hash_code, strong_value_type value)
    {
        size_t pos = which_bucket_(hash_code);
        size_t dist = 0;

        for (;;) {
            Bucket& bucket = buckets_[pos];

            // If the bucket is unoccupied, use it:
            if (!bucket.used_) {
                bucket.value_ = std::move(value);
                bucket.hash_code_ = hash_code;
                bucket.used_ = 1;
                ++size_;
                return;
            }

            // Check if the pointer is expired. If it is, use this slot.
            auto bucket_locked = bucket.value_.lock();
            auto bucket_key = weak_trait::key(bucket_locked);
            if (!bucket_key) {
                bucket.value_ = std::move(value);
                bucket.hash_code_ = hash_code;
                return;
            }

            // If not expired, but matches the value to insert, replace.
            auto key = weak_trait::key(value);
            if (hash_code == bucket.hash_code_ && equal_(*bucket_key, *key)) {
                bucket.value_ = std::move(value);
                return;
            }

            // Otherwise, we check the probe distance.
            size_t existing_distance =
                probe_distance_(pos, which_bucket_(bucket.hash_code_));
            if (dist > existing_distance) {
                bucket.value_ = std::exchange(value,
                                              weak_trait::move(bucket_locked));
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
};

template <
    class T,
    class Hash,
    class KeyEqual,
    class Allocator
>
class rh_weak_hash_table<T, Hash, KeyEqual, Allocator>::iterator
        : public std::iterator<std::forward_iterator_tag, T>
{
public:
    using base_t = typename vector_t::const_iterator;

    iterator(base_t start, base_t limit)
            : base_(start), limit_(limit)
    {
        find_next_();
    }

    strong_value_type operator*() const
    {
        return base_->value_.lock();
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

template <
    class Key,
    class Hash = std::hash<Key>,
    class KeyEqual = std::equal_to<Key>,
    class Allocator = std::allocator<Key>
>
using weak_unordered_set =
    rh_weak_hash_table<std::weak_ptr<const Key>, Hash, KeyEqual, Allocator>;

// template <class Key, class Value,
//           class Hash = std::hash<Key>,
//           class KeyEqual = std::equal_to<Key>,
//           class Allocator = std::allocator<weak_pair<Key, Value>>>
// using weak_unordered_map =
//     rh_weak_hash_table<weak_pair<Key, Value>, Hash, KeyEqual, Allocator>;

} // end namespace intersections::util
