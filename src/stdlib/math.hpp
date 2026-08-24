

#pragma once


// includes

#include <cmath>
#include <cstdlib>

#include "natives.hpp"


// functions

NATIVE(math_sqrt, "math", "sqrt", {
    p({
        {ANY_NUMERIC, true}
    });
    if(asNumberToDouble(args[0]) < 0) {
        vm->runtimeError("Seriously? Imaginary numbers?");
        return CaroNull;
    }
    return toFloat(args[0], [](auto&&... a) { return std::sqrt(decltype(a)(a)...); });
});
NATIVE_MATH(cbrt);

NATIVE_MATH(sin);
NATIVE_MATH(cos);
NATIVE_MATH(tan);

NATIVE_MATH(asin);
NATIVE_MATH(acos);
NATIVE_MATH(atan);
