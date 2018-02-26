#include "util/raw_vector.h"
#include <catch.hpp>
#include <string>

using namespace intersections::util;
using namespace std;

TEST_CASE("default construction")
{
    raw_vector<int> v;
    CHECK( v.size() == 0 );
}

TEST_CASE("int vector of 10")
{
    raw_vector<int> v(10);

    CHECK( v.size() == 10 );

    v[0] = 0;
    v[1] = 0;
    CHECK( v[0] == 0 );
    CHECK( v[1] == 0 );

    ++v[1];

    CHECK( v[0] == 0 );
    CHECK( v[1] == 1 );

    CHECK_THROWS_AS( v.at(12), std::range_error );
}

TEST_CASE("strings")
{
    raw_vector<string> v(10);

    new(&v[0]) string("hello");
    new(&v[1]) string("world");

    CHECK( v[0] == "hello" );
    CHECK( v[1] == "world" );

    v[0].~string();
    v[1].~string();
}
