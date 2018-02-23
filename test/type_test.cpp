#include "intersections.h"
#include "util/stringify.h"
#include <catch.hpp>

using namespace std;
using namespace intersections;

TEST_CASE("stringify(int_ty)")
{
    CHECK(stringify(type::make<int_ty>()) == "Int");
}