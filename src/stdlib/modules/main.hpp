
#pragma once


// INCLUDES

#include <chrono>
#include <thread>
#include <algorithm>
#include <numeric>

#include <cmath>
#include <cstdlib>

#include "../util/natives.hpp"

namespace chrono = std::chrono;
namespace ranges = std::ranges;


// GENERAL

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
        {{}, false},
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
        {{OBJ_STRING}, true },
        {{},           true },
        {{TYPE_BOOL},  false}
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
        {{OBJ_STRING}, true },
        {{TYPE_BOOL},  false}
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
        {{OBJ_STRING}, true}
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
        {{OBJ_STRING}, true}
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


// BASIC MATH

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


// CASTING


// the casting work

#define cantCast(target) {\
    vm->runtimeError("Can't cast %s to %s.", typeofValue(v).c_str(), string(target).c_str());\
    return CaroNull;\
}

Value castToNumber(VM* vm, const Value& v, ValueType targetType) {
    
    // easy to convert
    switch(v.type) {

        case TYPE_BOOL:   return CaroNumber(targetType, v.as.Abool? 1: 0);

        case TYPE_BYTE:   return CaroNumber(targetType, v.as.Abyte);
        case TYPE_INT:    return CaroNumber(targetType, v.as.Aint);
        case TYPE_UINT:   return CaroNumber(targetType, v.as.Auint);
        case TYPE_LONG:   return CaroNumber(targetType, v.as.Along);
        case TYPE_ULONG:  return CaroNumber(targetType, v.as.Aulong);
        case TYPE_FLOAT:  return CaroNumber(targetType, v.as.Afloat);
        case TYPE_DOUBLE: return CaroNumber(targetType, v.as.Adouble);
        
        default: break;
    }

    // strings
    if(isString(v)) {

        const string& str = asString(v)->str;
        double output;

        try{
            size_t length = 0;
            output = std::stod(str, &length);
            if(length < str.size()) cantCast(typeofType(targetType));    // garbage at the end
        } catch(...) {
            cantCast(typeofType(targetType));    // stod errored
        }

        return CaroNumber(targetType, output);

    }
    
    cantCast(typeofType(targetType));    // not a supported type

}

Value castToVector(VM* vm, ValueType targetType, const vector<Value>& args) {
    
    int       size = componentCount(targetType);
    ValueType type = componentType (targetType);
    vector<Value> components;

    // 1 argument, casting
    if(args.size() == 1) {

        const Value& v = args[0];

        // another vector
        if(isVector(v.type)) {
            if(componentCount(v.type) != size) cantCast(typeofType(targetType));    // no casting been vec2's and vec3's
            for(int i = 0; i < size; ++i) {
                components.push_back(getComponent(v, i));
            }

        // an array
        } else if(isArray(v)) {
            components = asArray(v)->data;
            if((int)components.size() != size) cantCast(typeofType(targetType));    // it needs to be the same size as the vector's length

        } else cantCast(typeofType(targetType));

    // same amount of components (either 2 or 3), so construct
    } else if((int)args.size() == size) {
        components = args;

    // neither of those
    } else {
        vm->runtimeError("%s takes 1 or %d parameters, but %d were given.", typeofType(targetType).c_str(), size, (int)args.size());
        return CaroNull;
    }

    // convert each component to the appropriate type
    components.resize(3, CaroInt(0));
    for(Value& value: components) {
        value = castToNumber(vm, value, type);
        if(vm->hadError) return CaroNull;
    }

    // return
    return CaroVector(
        targetType,
        asNumberTo<double>(components[0]), asNumberTo<double>(components[1]), asNumberTo<double>(components[2])
    );

}

Value castToArray(VM* vm, const Value& v) {

    // a string, split it
    if(isString(v)) {
        GCPause pause;
        vector<Value> data;
        const string& str = asString(v)->str;
        data.reserve(str.size());
        for(char c: str) {
            data.push_back(CaroObj(copyString(string(1, c))));
        }
        return CaroObj(copyArray(std::move(data)));
    }

    // a vector, spread it
    if(isVector(v.type)) {
        vector<Value> data;
        for(int i = 0; i < componentCount(v.type); ++i) {
            data.push_back(getComponent(v, i));
        }
        return CaroObj(copyArray(std::move(data)));
    }

    // an array, copy it
    if(isArray(v)) {
        return CaroObj(copyArray(asArray(v)->data));
    }

    // a set
    if(isSet(v)) {
        const unordered_set<Value>& items = asSet(v)->data;
        return CaroObj(copyArray(vector<Value>(items.begin(), items.end())));
    }

    cantCast("array");

}

Value castToDict(VM* vm, const Value& v) {
    // it just copies the dict
    if(isDict(v)) {
        return CaroObj(copyDict(asDict(v)->data));
    }
    cantCast("dict");
}

Value castToSet(VM* vm, const Value& v) {

    // copy
    if(isSet(v)) {
        return CaroObj(copySet(asSet(v)->data));
    }

    // an array
    if(isArray(v)) {
        GCPause pause;
        const vector<Value>& items = asArray(v)->data;
        unordered_set<Value> data;
        data.reserve(items.size());
        for(const Value& item: items) {
            if(!isValidKey(item)) {
                vm->runtimeError("Arrays, dicts, and sets currently can't be used as set items.");
                    // todo:
                return CaroNull;
            }
            data.insert(copyIfString(item));
        }
        return CaroObj(copySet(std::move(data)));
    }

    cantCast("set");

}



// actual casting functions

nFunc(main_bool, "", "bool", {
    params({{{}, true}});
    return CaroBool(isTruthy(args[0]));
});

#define nNumericCast(name, valueType)\
    nFunc(main_##name, "", #name, {\
        params({{{}, true}});\
        return castToNumber(vm, args[0], valueType);\
    })
nNumericCast(byte,   TYPE_BYTE);
nNumericCast(int,    TYPE_INT);
nNumericCast(uint,   TYPE_UINT);
nNumericCast(long,   TYPE_LONG);
nNumericCast(ulong,  TYPE_ULONG);
nNumericCast(float,  TYPE_FLOAT);
nNumericCast(double, TYPE_DOUBLE);

#define nVectorCast(name, valueType)\
    nFunc(main_##name, "", #name, {\
        return castToVector(vm, valueType, args);\
    })
nVectorCast(vec2i, TYPE_VEC2I);
nVectorCast(vec2u, TYPE_VEC2U);
nVectorCast(vec2f, TYPE_VEC2F);
nVectorCast(vec3i, TYPE_VEC3I);
nVectorCast(vec3u, TYPE_VEC3U);
nVectorCast(vec3f, TYPE_VEC3F);
    // also double as constructors
    
nFunc(main_str, "", "str", {
    params({
        {{},          true },
        {ANY_NUMERIC, false}
    });

    // number with base
    if(args.size() >= 2) {

        if(!isInt(args[0].type)) {
            vm->runtimeError("The base can only be specified when the value is an integer.");
            return CaroNull;
        }

        uint32_t base = asNumberTo<uint32_t>(args[1]);
        if(base != 2 && base != 8 && base != 10 && base != 16) {
            vm->runtimeError("The base can only be 2, 8, 10, or 16, but %d was given.", base);
            return CaroNull;
        }

        auto inBase = [&](auto number) -> string {
            switch(base) {
                case 2:  return format("{:b}", number);
                case 8:  return format("{:o}", number);
                case 16: return format("{:X}", number);    // {:X} outputs the letters as capital letters
                default: return format("{:d}", number);
            }
        };

        return CaroObj(copyString(
            isUnsigned(args[0].type)? inBase(asNumberTo<uint64_t>(args[0])): inBase(asNumberTo<int64_t> (args[0]))
        ));

    }

    // everything else
    return CaroObj(copyString(printValue(args[0])));

});

nFunc(main_array, "", "array", {
    params({{{}, true}});
    return castToArray(vm, args[0]);
});
nFunc(main_dict, "", "dict", {
    params({{{}, true}});
    return castToDict(vm, args[0]);
});
nFunc(main_set, "", "set", {
    params({{{}, true}});
    return castToSet(vm, args[0]);
});

