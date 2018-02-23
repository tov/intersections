#include "intersections.h"
#include "util/stringify.h"
#include <catch.hpp>

using namespace std;
using namespace intersections;

TEST_CASE("stringify(int_ty)")
{
    CHECK(stringify(type::make<int_ty>()) == "Int");
    CHECK(stringify(type::make<function_ty>(vector{type::make<int_ty>()},
                                            type::make<double_ty>()))
          == "(Int) -> Double");
    CHECK(stringify(type::make<function_ty>(vector{type::make<int_ty>(),
                                                   type::make<real_ty>()},
                                            type::make<double_ty>()))
          == "(Int, Real) -> Double");
}