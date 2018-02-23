#pragma once

#include <iostream>

template <class Container>
class Separated
{
public:
    Separated(const Container& container) : container_(container) {}

private:
    Container const& container_;

    friend
    std::ostream& operator<<(std::ostream& o, const Separated<Container>& wc)
    {
        bool first_time = true;

        for (auto const& element : wc.container_) {
            if (first_time) {
                first_time = false;
            } else {
                o << ", ";
            }

            o << element;
        }

        return o;
    }
};

