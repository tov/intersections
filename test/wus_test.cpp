#include "util/weak_unordered_set.h"
#include <catch.hpp>
#include <memory>
#include <vector>

using namespace std;
using namespace intersections::util;

TEST_CASE("Default construction and insertion")
{
    weak_unordered_set<int> set;

    auto five = make_shared<int>(5);
    set.insert(five);

    CHECK( set.member(5) );
    CHECK_FALSE( set.member(6) );

    vector<int> actual;
    for (auto ptr : set)
        actual.push_back(*ptr);
    CHECK( actual == vector{5} );

    five = nullptr;

    CHECK_FALSE( set.member(5) );
    CHECK_FALSE( set.member(6) );
}

TEST_CASE("growing")
{
    vector<shared_ptr<int>> holder;
    weak_unordered_set<int> set;

    for (size_t i = 0; i < 1000; ++i) {
        auto new_ptr = make_shared<int>(i);
        holder.push_back(new_ptr);
        set.insert(new_ptr);
    }

    CHECK( 1000 == set.size() );
    CHECK( set.member(999) );
    holder.pop_back();
    CHECK( 1000 == set.size() );
    CHECK( !set.member(999) );

    size_t count = 0;
    for (const auto& ptr : set) ++count;
    CHECK( count == 999 );

    CHECK( 1000 == set.size() );
}
