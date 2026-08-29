
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
        return CaroNull;
    }
    return CaroObj(copyString(vm->appName));
});
NATIVE(desc, "", "desc", {
    if(vm->appDesc.empty()) {
        vm->runtimeError("This app doesn't have a description.");
        return CaroNull;
    }
    return CaroObj(copyString(vm->appDesc));
});
NATIVE(version, "", "version", {
    if(vm->appVersion.empty()) {
        vm->runtimeError("This app doesn't have a version.");
        return CaroNull;
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

NATIVE(input, "", "input", {
    params({
        {{TYPE_OBJ}, false},
    });

    if(args.size() >= 1) {
        cout << asString(args[0])->str;
    }
    string result;
    std::getline(cin, result);
    return CaroObj(copyString(result));

});

NATIVE(log, "", "log", {
    params({
        {{TYPE_OBJ},  true },
        {{},          true },
        {{TYPE_BOOL}, false}
    });

    string logType = asString(args[0])->str;
    if(!(logType == "e" || logType == "w" || logType == "i")) {
        vm->runtimeError("The only valid log types are e, w, and i.");
        return CaroNull;
    }
    uint8_t color;
    switch(logType.front()) {
        case 'e': color = 91; break;
        case 'w': color = 93; break;
        case 'i': color = 96; break;
    }
    string bold = args.size() >= 3 && args[2].as.Abool? "\033[1m": "";

    auto now = chrono::floor<chrono::milliseconds>(chrono::system_clock::now());
    chrono::hh_mm_ss hms{now - floor<chrono::days>(now)};
    cout << format(
                "{:s}\033[{}m[{:c}] [{:02}:{:02}:{:02}.{:03}] ",
                bold, color, logType.front(),
                hms.hours().count(), hms.minutes().count(), hms.seconds().count(), hms.subseconds().count()
            );
    printValue(args[1]);
    cout << "\033[0m\n";

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
    std::this_thread::sleep_for(chrono::duration<double>(asNumberTo<double>(args[0])));
    return CaroNull;

});
NATIVE(wait_ms, "", "wait_ms", {
    params({
        {ANY_NUMERIC, true}
    });
    std::this_thread::sleep_for(chrono::duration<double, std::milli>(asNumberTo<double>(args[0])));
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

