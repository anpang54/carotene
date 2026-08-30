
#pragma once


// includes

#include <chrono>
#include <thread>
#include <algorithm>
#include <numeric>

#include <cmath>
#include <cstdlib>

#include "natives.hpp"

namespace chrono = std::chrono;
namespace ranges = std::ranges;


// general

NATIVE(main_name, "", "name", {
    if(vm->appName.empty()) {
        vm->runtimeError("This app doesn't have a name.");
        return CaroNull;
    }
    return CaroObj(copyString(vm->appName));
});
NATIVE(main_desc, "", "desc", {
    if(vm->appDesc.empty()) {
        vm->runtimeError("This app doesn't have a description.");
        return CaroNull;
    }
    return CaroObj(copyString(vm->appDesc));
});
NATIVE(main_version, "", "version", {
    if(vm->appVersion.empty()) {
        vm->runtimeError("This app doesn't have a version.");
        return CaroNull;
    }
    return CaroObj(copyString(vm->appVersion));
});

NATIVE(main_caro_version, "", "caro_version", {
    return CaroObj(copyString(VERSION));
});
NATIVE(main_caro_version_date, "", "caro_version_date", {
    return CaroObj(copyString(VERSION_DATE));
});

NATIVE(main_print, "", "print", {
    params({
        {{},          true },
        {{TYPE_BOOL}, false}
    });

    printValue(args[0]);
    if(args.size() < 2 || !isFalsy(args[1])) {
        cout << '\n';
    }
    return CaroNull;

});

NATIVE(main_input, "", "input", {
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

NATIVE(main_log, "", "log", {
    params({
        {{TYPE_OBJ},  true },
        {{},          true },
        {{TYPE_BOOL}, false}
    });

    string logType = asString(args[0])->str;
    if(!(logType == "e" || logType == "w" || logType == "o" || logType == "i")) {
        vm->runtimeError("The only valid log types are e, w, o, and i.");
        return CaroNull;
    }
    uint8_t color;
    switch(logType.front()) {
        case 'e': color = 203; break;
        case 'w': color = 221; break;
        case 'o': color = 82;  break;
        case 'i': color = 45;  break;
    }
    string bold = args.size() >= 3 && args[2].as.Abool? "\033[1m": "";

    auto now = chrono::floor<chrono::milliseconds>(chrono::system_clock::now());
    chrono::hh_mm_ss hms{now - floor<chrono::days>(now)};
    cout << format(
                "{:s}\033[38;5;{}m[{:c}] [{:02}:{:02}:{:02}.{:03}] ",
                bold, color, logType.front(),
                hms.hours().count(), hms.minutes().count(), hms.seconds().count(), hms.subseconds().count()
            );
    printValue(args[1]);
    cout << "\033[0m\n";

    return CaroNull;

});

NATIVE(main_sh, "", "sh", {
    params({
        {{TYPE_OBJ}, true}
    });

    int output = std::system(asString(args[0])->str.c_str());
    return CaroInt(output);

});

NATIVE(main_wait, "", "wait", {
    params({
        {ANY_NUMERIC, true}
    });
    std::this_thread::sleep_for(chrono::duration<double>(asNumberTo<double>(args[0])));
    return CaroNull;

});
NATIVE(main_wait_ms, "", "wait_ms", {
    params({
        {ANY_NUMERIC, true}
    });
    std::this_thread::sleep_for(chrono::duration<double, std::milli>(asNumberTo<double>(args[0])));
    return CaroNull;
});

NATIVE(main_exit, "", "exit", {
    params({
        {{TYPE_BOOL}, false}
    });
    exit(args.size() == 0 || isFalsy(args[0])? 0: 1);
});

NATIVE(main_clock, "", "clock", {
    params({});
    return CaroDouble((double)clock() / CLOCKS_PER_SEC);
});
    // will probably delete once done with crafting interpreters


// basic math

NATIVE(main_abs, "", "abs", {
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
    NATIVE(main_##name, "", #name, {\
        params({\
            {ANY_NUMERIC, true}\
        });\
        return mapFloat(args[0], [](auto&&... a) { return std::name(decltype(a)(a)...); });\
    })

NATIVE_ROUNDING(floor);
NATIVE_ROUNDING(ceil);
NATIVE_ROUNDING(round);

#define NATIVE_ARRAY_STAT(name, ...)\
    NATIVE(main_##name, "", #name, {\
        params({\
            {{TYPE_OBJ}, true}\
        });\
        \
        vector<Value>& data = asArray(args[0])->data;\
        \
        if(data.empty()) {\
            vm->runtimeError("That array is empty.");\
            return CaroNull;\
        }\
        \
        vector<double> values;\
        values.reserve(data.size());\
        for(const Value& number: data) {\
            if(!isNumeric(number.type)) {\
                vm->runtimeError("Every item should be numeric, but there's %s.", typeofValue(number).c_str());\
                return CaroNull;\
            }\
            values.push_back(asNumberTo<double>(number));\
        }\
        \
        auto stat = [](vector<double>& values) -> double __VA_ARGS__;\
        return CaroDouble(stat(values));\
        \
    })

NATIVE_ARRAY_STAT(min, {
    return ranges::min(values);
});
NATIVE_ARRAY_STAT(max, {
    return ranges::max(values);
});
NATIVE_ARRAY_STAT(mean, {
    return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
});
NATIVE_ARRAY_STAT(median, {
    ranges::sort(values);
    size_t middle = values.size() / 2;
    if(values.size() % 2 == 0) {    // even number of elements, so take the average
        return (values[middle - 1] + values[middle]) / 2;
    }
    return values[middle];
});

