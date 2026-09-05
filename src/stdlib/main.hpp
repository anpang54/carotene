
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

nFunc(main_name, "", "name", {
    if(vm->appName.empty()) {
        vm->runtimeError("This app doesn't have a name.");
        return CaroNull;
    }
    return CaroObj(copyString(vm->appName));
});
nFunc(main_desc, "", "desc", {
    if(vm->appDesc.empty()) {
        vm->runtimeError("This app doesn't have a description.");
        return CaroNull;
    }
    return CaroObj(copyString(vm->appDesc));
});
nFunc(main_version, "", "version", {
    if(vm->appVersion.empty()) {
        vm->runtimeError("This app doesn't have a version.");
        return CaroNull;
    }
    return CaroObj(copyString(vm->appVersion));
});

nFunc(main_caro_version, "", "caro_version", {
    return CaroObj(copyString(VERSION));
});
nFunc(main_caro_version_date, "", "caro_version_date", {
    return CaroObj(copyString(VERSION_DATE));
});

nFunc(main_platform, "", "platform", {
    params({});

    string platform;

    #if defined(__EMSCRIPTEN__)
        // emscripten larps as unix, which yeah it doesn't affect any of the real OSes below but better safe than sorry
        platform = "web";
    #elif defined(_WIN32)
        platform = "windows";
    #elif defined(__APPLE__)
        platform = "macos";
    #elif defined(__linux__)
        platform = "linux";
    #elif defined(__FreeBSD__)
        platform = "freebsd";
    #elif defined(__HAIKU__)
        platform = "haiku";
    #else
        platform = "unknown";
    #endif
        // wow, zero harmony between how each platform defines its macro, how beautiful
        
    return CaroObj(copyString(platform));

});

nFunc(main_print, "", "print", {
    params({
        {{},          true },
        {{TYPE_BOOL}, false}
    });

    #ifdef __EMSCRIPTEN__
        if(isString(args[0]) && asString(args[0])->fString) {
            auto [text, cssRules] = formatString(asString(args[0])->str);
            string js = "console.log(\"" + escapeJS(text) + "\"";
            for(const string& rule: cssRules) {
                js += ", \"" + escapeJS(rule) + "\"";
            }
            js += ")";
            runJS(js);
            return CaroNull;
        }
    #endif

    cout << printValue(args[0]);
    if(args.size() < 2 || !isFalsy(args[1])) {
        cout << '\n';
    }
    return CaroNull;

});

nFunc(main_input, "", "input", {
    params({
        {{TYPE_OBJ}, false},
    });

    if(args.size() >= 1) {
        cout << printValue(args[0]);
    }
    string result;
    std::getline(cin, result);
    return CaroObj(copyString(result));

});

nFunc(main_log, "", "log", {
    params({
        {{TYPE_OBJ},  true },
        {{},          true },
        {{TYPE_BOOL}, false}
    });

    // check type
    string logType = asString(args[0])->str;
    if(!(logType == "e" || logType == "w" || logType == "o" || logType == "i")) {
        vm->runtimeError("The only valid log types are e, w, o, and i.");
        return CaroNull;
    }

    // get main text
    auto now = chrono::floor<chrono::milliseconds>(chrono::system_clock::now());
    chrono::hh_mm_ss hms{now - floor<chrono::days>(now)};
    string mainText = format(
                          "[{:c}] [{:02}:{:02}:{:02}.{:03}] ",
                          logType.front(), hms.hours().count(), hms.minutes().count(), hms.seconds().count(), hms.subseconds().count()
                      );

    // format
    string color;
    bool bold = args.size() >= 3 && isTruthy(args[2]);
    #ifdef __EMSCRIPTEN__

        // format for devtools console using CSS
        switch(logType.front()) {
            case 'e': color = "#ff5f5f"; break;
            case 'w': color = "#ffd75f"; break;
            case 'o': color = "#5fff00"; break;
            case 'i': color = "#00d7ff"; break;
            // hex codes are from https://color-palette.hexdocs.pm/ansi_color_codes.html
            // has the # so that vscode gives a fancy color box
        }
        string css = "color: " + color + (bold? "; font-weight: bold": "");
        runJS("console.log(\"%c" + mainText + printValue(args[1]) + "\", \"" + css + "\")");

    #else

        // format for terminals using ANSI escape codes
        switch(logType.front()) {
            case 'e': color = "203"; break;
            case 'w': color = "221"; break;
            case 'o': color = "82";  break;
            case 'i': color = "45";  break;
        }
        cout << "\033[38;5;" << color << "m" << (bold? "\033[1m": "") << mainText << printValue(args[1]) << "\033[0m\n";

    #endif

    return CaroNull;

});

nFunc(main_sh, "", "sh", {
    params({
        {{TYPE_OBJ},  true },
        {{TYPE_BOOL}, false}
    });

    #ifdef __EMSCRIPTEN__
        vm->runtimeError("sh() is only available on non-web platforms.");
        return CaroNull;
    #else

        FILE* outputFile = popen(asString(args[0])->str.c_str(), "r");
        if(!outputFile) {
            vm->runtimeError("Command failed.");
            return CaroNull;
        }

        string outputText;
        char buffer[256];
        bool autoPrint = args.size() >= 2 && isTruthy(args[1]);
        while(fgets(buffer, sizeof(buffer), outputFile)) {
            if(autoPrint) cout << buffer << std::flush;
            outputText += buffer;
        }

        pclose(outputFile);

        return CaroObj(copyString(outputText));

    #endif

});
nFunc(main_js, "", "js", {
    params({
        {{TYPE_OBJ}, true}
    });

    #ifdef __EMSCRIPTEN__
        string result = runJS(asString(args[0])->str);
        return CaroObj(copyString(result));
    #else
        vm->runtimeError("js() is only available on web.");
        return CaroNull;
    #endif

});

nFunc(main_eval, "", "eval", {
    params({
        {{TYPE_OBJ}, true}
    });
    return vm->eval(asString(args[0])->str);
});

nFunc(main_wait, "", "wait", {
    params({
        {ANY_NUMERIC, true}
    });
    std::this_thread::sleep_for(chrono::duration<double>(asNumberTo<double>(args[0])));
    return CaroNull;

});
nFunc(main_wait_ms, "", "wait_ms", {
    params({
        {ANY_NUMERIC, true}
    });
    std::this_thread::sleep_for(chrono::duration<double, std::milli>(asNumberTo<double>(args[0])));
    return CaroNull;
});

nFunc(main_exit, "", "exit", {
    params({
        {{TYPE_BOOL}, false}
    });
    exit(args.size() == 0 || isFalsy(args[0])? 0: 1);
});

nFunc(main_clock, "", "clock", {
    params({});
    return CaroDouble((double)clock() / CLOCKS_PER_SEC);
});
    // will probably delete once done with crafting interpreters


// basic math

nFunc(main_abs, "", "abs", {
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

#define nRounding(name)\
    nFunc(main_##name, "", #name, {\
        params({\
            {ANY_NUMERIC, true}\
        });\
        return mapFloat(args[0], [](auto&&... a) { return std::name(decltype(a)(a)...); });\
    })

nRounding(floor);
nRounding(ceil);
nRounding(round);

nArrayStatAny(min, {
    return ranges::min(values);
});
nArrayStatAny(max, {
    return ranges::max(values);
});
nArrayStatAny(mean, {
    return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
});
nArrayStatAny(median, {
    ranges::sort(values);
    size_t middle = values.size() / 2;
    if(values.size() % 2 == 0) {    // even number of elements, so take the average
        return (values[middle - 1] + values[middle]) / 2;
    }
    return values[middle];
});


// vector "constructors" until I add actual constructors

nFunc(main_vec2i, "", "vec2i", {
    params({ {{TYPE_INT}, true}, {{TYPE_INT}, true} });
    return CaroVec2i(args[0].as.Aint, args[1].as.Aint);
});
nFunc(main_vec2u, "", "vec2u", {
    params({ {{TYPE_UINT}, true}, {{TYPE_UINT}, true} });
    return CaroVec2u(args[0].as.Auint, args[1].as.Auint);
});
nFunc(main_vec2f, "", "vec2f", {
    params({ {{TYPE_FLOAT}, true}, {{TYPE_FLOAT}, true} });
    return CaroVec2f(args[0].as.Afloat, args[1].as.Afloat);
});

nFunc(main_vec3i, "", "vec3i", {
    params({ {{TYPE_INT}, true}, {{TYPE_INT}, true}, {{TYPE_INT}, true} });
    return CaroVec3i(args[0].as.Aint, args[1].as.Aint, args[2].as.Aint);
});
nFunc(main_vec3u, "", "vec3u", {
    params({ {{TYPE_UINT}, true}, {{TYPE_UINT}, true}, {{TYPE_UINT}, true} });
    return CaroVec3u(args[0].as.Auint, args[1].as.Auint, args[2].as.Auint);
});
nFunc(main_vec3f, "", "vec3f", {
    params({ {{TYPE_FLOAT}, true}, {{TYPE_FLOAT}, true}, {{TYPE_FLOAT}, true} });
    return CaroVec3f(args[0].as.Afloat, args[1].as.Afloat, args[2].as.Afloat);
});

