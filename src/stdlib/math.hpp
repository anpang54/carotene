

#pragma once


// includes

#include <cmath>
#include <cstdlib>

#include "natives.hpp"


// functions

#define NATIVE_MATH(name)\
    NATIVE(math_##name, "math", #name, {\
        params({\
            {ANY_NUMERIC, true}\
        });\
        return toFloat(args[0], [](auto&&... a) { return std::name(decltype(a)(a)...); });\
    })

NATIVE(math_sqrt, "math", "sqrt", {
    params({
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
