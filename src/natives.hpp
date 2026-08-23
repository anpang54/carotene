
#pragma once


// includes

#include <cmath>
#include <cstdlib>

#include "common.hpp"


// macros to help shorten stuff

#define NATIVE(cppName, caroName, ...)\
    DefineNative N_##cppName(caroName, [](VM* vm, vector<Value> args) -> Value __VA_ARGS__)
    // every native function has the same C++ function signature soo

#define NATIVE_ROUNDING(name)\
    NATIVE(name, #name, {\
        p({\
            {ANY_NUMERIC, true}\
        });\
        return mapFloat(args[0], [](auto&&... a) { return std::name(decltype(a)(a)...); });\
    })

#define NATIVE_MATH(name)\
    NATIVE(math_##name, #name, {\
        p({\
            {ANY_NUMERIC, true}\
        });\
        return toFloat(args[0], [](auto&&... a) { return std::name(decltype(a)(a)...); });\
    })


// NO NAMESPACE


// general

NATIVE(print, "print", {
    p({
        {{},          true },
        {{TYPE_BOOL}, false}
    });
    printValue(args[0]);
    if(args.size() < 2 || !isFalsey(args[1])) {
        cout << '\n';
    }
    return CaroNull;
});

NATIVE(sh, "sh", {
    p({
        {{TYPE_OBJ}, true}
    });
    int output = std::system(asString(args[0])->str.c_str());
    return CaroInt(output);
});

NATIVE(clock, "clock", {
    p({});
    return CaroDouble((double)clock() / CLOCKS_PER_SEC);
});
    // will probably delete once done with crafting interpreteres


// basic math

NATIVE(abs, "abs", {
    p({
        {ANY_NUMERIC, true}
    });
    return mapNumber(args[0], [](auto x) -> decltype(x) {
        using T = decltype(x);
        if constexpr(std::is_unsigned_v<T>) {
            return x;    // already unsigned
        } else if constexpr(std::is_floating_point_v<T>) {
            return std::fabs(x);
        } else {
            return x < 0? (T)(-(std::make_unsigned_t<T>)x): x;
        }
    });
});

NATIVE_ROUNDING(floor);
NATIVE_ROUNDING(ceil);
NATIVE_ROUNDING(round);

// todo: min, max, mean, etc. once we have arrays


// MATH

// once namespaces are actually added, these'll be math.sqrt, math.cbrt, etc.

NATIVE(math_sqrt, "sqrt", {
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
