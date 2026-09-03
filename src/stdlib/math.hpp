

#pragma once


// includes

#include <cmath>
#include <cstdlib>
#include <numbers>
#include <limits>

#include "natives.hpp"

namespace numbers = std::numbers;
using std::numeric_limits;


// functions

#define nMath(name)\
    nFunc(math_##name, "math", #name, {\
        params({\
            {ANY_NUMERIC, true}\
        });\
        return toFloat(args[0], [](auto&&... a) { return std::name(decltype(a)(a)...); });\
    })

nFunc(math_sqrt, "math", "sqrt", {
    params({
        {ANY_NUMERIC, true}
    });
    if(asNumberTo<double>(args[0]) < 0) {
        vm->runtimeError("Seriously? Imaginary numbers?");
        return CaroNull;
    }
    return toFloat(args[0], [](auto&&... a) { return std::sqrt(decltype(a)(a)...); });
});
nMath(cbrt);

nMath(sin);
nMath(cos);
nMath(tan);

nMath(asin);
nMath(acos);
nMath(atan);

nArrayStatInt(gcd, {
    uint64_t result = 0;
    for(int64_t value: values) {
        result = std::gcd(result, value);
    }
    return (int64_t)result;
});
nArrayStatInt(lcm, {
    uint64_t result = 1;
    for(int64_t value: values) {
        if(value == 0) return 0;
        uint64_t magnitude = value;
        result = result / std::gcd(result, magnitude) * magnitude;
    }
    return (int64_t)result;
});

nFunc(math_to_degrees, "math", "to_degrees", {
    params({
        {ANY_NUMERIC, true}
    });
    return toFloat(args[0], [](auto&& a) { return decltype(a)(a) * (180.0 / numbers::pi); });
});
nFunc(math_to_radians, "math", "to_radians", {
    params({
        {ANY_NUMERIC, true}
    });
    return toFloat(args[0], [](auto&& a) { return decltype(a)(a) * (numbers::pi / 180.0); });
});

// constants

nConst(math_inf, "math", "inf", { return CaroDouble(INFINITY);        });
nConst(math_nan, "math", "nan", { return CaroDouble(std::nan(""));    });

nConst(math_pi,  "math", "pi",  { return CaroDouble(numbers::pi);     });
nConst(math_tau, "math", "tau", { return CaroDouble(numbers::pi * 2); });
nConst(math_e,   "math", "e",   { return CaroDouble(numbers::e);      });
nConst(math_phi, "math", "phi", { return CaroDouble(numbers::phi);    });

nConst(math_int_min,        "math", "int_min",        { return CaroInt   (numeric_limits< int32_t>::min());     });
nConst(math_int_max,        "math", "int_max",        { return CaroInt   (numeric_limits< int32_t>::max());     });
nConst(math_uint_min,       "math", "uint_min",       { return CaroUint  (numeric_limits<uint32_t>::min());     });
nConst(math_uint_max,       "math", "uint_max",       { return CaroUint  (numeric_limits<uint32_t>::max());     });
nConst(math_long_min,       "math", "long_min",       { return CaroLong  (numeric_limits< int64_t>::min());     });
nConst(math_long_max,       "math", "long_max",       { return CaroLong  (numeric_limits< int64_t>::max());     });
nConst(math_ulong_min,      "math", "ulong_min",      { return CaroUlong (numeric_limits<uint64_t>::min());     });
nConst(math_ulong_max,      "math", "ulong_max",      { return CaroUlong (numeric_limits<uint64_t>::max());     });
nConst(math_float_min,      "math", "float_min",      { return CaroFloat (numeric_limits<float   >::lowest());  });
nConst(math_float_max,      "math", "float_max",      { return CaroFloat (numeric_limits<float   >::max());     });
nConst(math_float_epsilon,  "math", "float_epsilon",  { return CaroFloat (numeric_limits<float   >::epsilon()); });
nConst(math_double_min,     "math", "double_min",     { return CaroDouble(numeric_limits<double  >::lowest());  });
nConst(math_double_max,     "math", "double_max",     { return CaroDouble(numeric_limits<double  >::max());     });
nConst(math_double_epsilon, "math", "double_epsilon", { return CaroDouble(numeric_limits<double  >::epsilon()); });
