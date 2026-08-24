
#pragma once


// includes

#include <cmath>
#include <cstdlib>

#include "natives.hpp"


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

