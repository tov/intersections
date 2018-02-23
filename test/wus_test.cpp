#include "util/weak_unordered_set.h"
#include <catch.hpp>

using namespace weak;

TEST_CASE("Default construction")
{
    probing_weak_unordered_set<int> set;
}