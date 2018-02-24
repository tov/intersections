#include "util/weak_unordered_set.h"
#include <catch.hpp>
#include <memory>
#include <vector>

using namespace std;
using namespace intersections::util;

TEST_CASE("Default construction and insertion")
{
    rh_weak_unordered_set<int> set;

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
