
#pragma once


// includes

#include <chrono>
#include <thread>

#include <cmath>
#include <cstdlib>

#include "natives.hpp"

namespace chrono = std::chrono;


// general

NATIVE(name, "", "name", {
    if(vm->appName.empty()) {
        vm->runtimeError("This app doesn't have a name.");
    }
    return CaroObj(copyString(vm->appName));
});
NATIVE(desc, "", "desc", {
    if(vm->appDesc.empty()) {
        vm->runtimeError("This app doesn't have a description.");
    }
    return CaroObj(copyString(vm->appDesc));
});
NATIVE(version, "", "version", {
    if(vm->appVersion.empty()) {
        vm->runtimeError("This app doesn't have a version.");
    }
    return CaroObj(copyString(vm->appVersion));
});

NATIVE(print, "", "print", {
    params({
        {{},          true },
        {{TYPE_BOOL}, false}
    });
    printValue(args[0]);
    if(args.size() < 2 || !isFalsey(args[1])) {
        cout << '\n';
    }
    return CaroNull;
});

NATIVE(sh, "", "sh", {
    params({
        {{TYPE_OBJ}, true}
    });
    int output = std::system(asString(args[0])->str.c_str());
    return CaroInt(output);
});

NATIVE(wait, "", "wait", {
    params({
        {ANY_NUMERIC, true}
    });
    std::this_thread::sleep_for(chrono::duration<double>(asNumberToDouble(args[0])));
    return CaroNull;
});
NATIVE(wait_ms, "", "wait_ms", {
    params({
        {ANY_NUMERIC, true}
    });
    std::this_thread::sleep_for(chrono::duration<double, std::milli>(asNumberToDouble(args[0])));
    return CaroNull;
});

NATIVE(clock, "", "clock", {
    params({});
    return CaroDouble((double)clock() / CLOCKS_PER_SEC);
});
    // will probably delete once done with crafting interpreters


// basic math

NATIVE(abs, "", "abs", {
    params({
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

#define NATIVE_ROUNDING(name)\
    NATIVE(name, "", #name, {\
        params({\
            {ANY_NUMERIC, true}\
        });\
        return mapFloat(args[0], [](auto&&... a) { return std::name(decltype(a)(a)...); });\
    })

NATIVE_ROUNDING(floor);
NATIVE_ROUNDING(ceil);
NATIVE_ROUNDING(round);

// todo: min, max, mean, etc. once we have arrays

