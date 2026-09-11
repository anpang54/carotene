
#pragma once


// INCLUDES

#include <cctype>
#include <climits>

#include "../util/natives.hpp"


// MACROS

#define SELF    renderString(asString(self))
#define ARGS(n) renderString(asString(args[n]))


// METHODS


// getting info

nBuiltin(OBJ_STRING, length, {
    params({});
    return CaroUint(SELF.size());
});


// transformations

nBuiltin(OBJ_STRING, lower, {
    params({});
    return CaroObj(copyString(lower(SELF)));
});

nBuiltin(OBJ_STRING, upper, {
    params({});
    return CaroObj(copyString(upper(SELF)));
});

nBuiltin(OBJ_STRING, capitalize, {
    params({});
    string result = SELF;
    if(!result.empty()) {
        result[0] = std::toupper(static_cast<unsigned char>(result[0]));
    }
    return CaroObj(copyString(std::move(result)));
});

nBuiltin(OBJ_STRING, replace, {
    params({
        {{OBJ_STRING}, true },
        {{OBJ_STRING}, true },
        {ANY_NUMERIC,  false}
    });

    int64_t count = args.size() >= 3? asNumberTo<int64_t>(args[2]): 0;    // 0 = replace all occurrences
    if(count < 0) {
        vm->runtimeError("The replacement count can't be negative.");
        return CaroNull;
    }
    if(count > INT_MAX) count = 0;

    string result = SELF;
    replace(result, ARGS(0), ARGS(1), static_cast<int>(count));
    return CaroObj(copyString(std::move(result)));

});

nBuiltin(OBJ_STRING, trim, {
    params({});
    return CaroObj(copyString(trim(SELF)));
});


#undef SELF
#undef ARGS
