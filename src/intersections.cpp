#include "intersections.h"
#include "util/Separated.h"

namespace intersections {

std::ostream& operator<<(std::ostream& o, const type& ty)
{
    ty.pimpl_->format(o);
    return o;
}

void int_ty::format(std::ostream& o) const
{
    o << "Int";
}

void double_ty::format(std::ostream& o) const
{
    o << "Double";
}

void real_ty::format(std::ostream& o) const
{
    o << "Real";
}

void function_ty::format(std::ostream& o) const
{
    o << '(' << Separated{arguments} << ") -> " << result;
}

} // end namespace intersections

