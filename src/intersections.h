#pragma once

#include <iostream>
#include <memory>
#include <variant>
#include <vector>

namespace intersections {

struct type_impl_base {
    virtual void format(std::ostream&) const = 0;
    virtual ~type_impl_base() = default;
};

class type {
public:
    using pimpl_t = std::shared_ptr<type_impl_base>;

    template <class Derived, class... Args>
    static type make(Args&&... args)
    {
        return type(std::make_shared<Derived>(std::forward<Args>(args)...));
    }

private:
    pimpl_t pimpl_;

    explicit type(pimpl_t pimpl) : pimpl_(std::move(pimpl)) {}

    friend std::ostream& operator<<(std::ostream&, const type&);
};


struct int_ty : type_impl_base {
    void format(std::ostream&) const override;
};

struct double_ty : type_impl_base {
    void format(std::ostream&) const override;
};

struct real_ty : type_impl_base {
    void format(std::ostream&) const override;
};

struct function_ty : type_impl_base {
    function_ty(std::vector<type>, type);

    std::vector<type> arguments;
    type result;

    void format(std::ostream&) const override;
};

} // end namespace intersections