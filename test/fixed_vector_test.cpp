#include "util/fixed_vector.h"
#include <catch.hpp>

using namespace intersections::util;

TEST_CASE("default construction")
{
    fixed_vector<int> v;
    CHECK( v.size() == 0 );
}

TEST_CASE("10 zeros")
{
    fixed_vector<int> v(10);

    CHECK( v.size() == 10 );
    CHECK( v[0] == 0 );
    CHECK( v[1] == 0 );

    ++v[1];

    CHECK( v[0] == 0 );
    CHECK( v[1] == 1 );

    CHECK_THROWS_AS( v.at(12), std::range_error );
}