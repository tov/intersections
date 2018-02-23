#pragma once

#include "fixed_vector.h"

#include <memory>

namespace intersections::util {

static size_t const default_bucket_count = 8;

template <
    class Key,
    class Hash = std::hash<Key>,
    class KeyEqual = std::equal_to<Key>,
    class Allocator = std::allocator<Key>
>
class probing_weak_unordered_set
{
public:
    using value_type = Key;
    using weak_ptr_type = std::weak_ptr<const value_type>;
    using hasher = Hash;
    using key_equal = KeyEqual;
    using allocator_type = Allocator;
private:
    using bucket_t = std::pair<std::weak_ptr<const Key>, const size_t>;
public:
    using bucket_allocator_type =
        typename std::allocator_traits<allocator_type>::template rebind_alloc<bucket_t>;

    probing_weak_unordered_set()
            : probing_weak_unordered_set(default_bucket_count)
    { }

    explicit probing_weak_unordered_set(
        size_t capacity,
        const hasher& hash = hasher(),
        const key_equal& equal = key_equal(),
        const allocator_type& allocator = allocator_type(),
        const bucket_allocator_type& bucket_allocator = bucket_allocator_type())
            : hash_(hash)
            , equal_(equal)
            , allocator_(allocator)
            , bucket_allocator_(bucket_allocator)
            , buckets_(capacity, bucket_allocator_)
    { }

private:
    hasher hash_;
    key_equal equal_;
    allocator_type allocator_;
    bucket_allocator_type bucket_allocator_;
    fixed_vector<bucket_t, bucket_allocator_type> buckets_;
};

} // end namespace intersections::util
