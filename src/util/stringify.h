#pragma once

#include <sstream>
#include <string>

template <class T>
std::string stringify(const T& thing)
{
    std::ostringstream o;
    o << thing;
    return o.str();
}