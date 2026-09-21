

#pragma once


// INCLUDES

#include <cctype>

#include "../util/natives.hpp"
#include "../util/json_parser.hpp"


// FUNCTIONS

nFunc(json_stringify, "json", "stringify", {
    params({
        {{}, true}
    });
    return CaroObj(copyString(printValue(args[0], true)));
});
    // currently print(json.stringify([1, 2, 3])) prints nothing because print() treats [1,2,3] as a string formatting tag

nFunc(json_parse, "json", "parse", {
    params({
        {{OBJ_STRING}, true}
    });

    GCPause pause;

    CaroJsonParser parser(asString(args[0])->str);
    Value parsed = parser.parseValue();
    parser.skipWhitespace();

    if(parser.errored) {
        vm->runtimeError("Invalid JSON.");
        return CaroNull;
    }
    if(parser.i < parser.text.length()) {
        vm->runtimeError("Trailing garbage after JSON.");
        return CaroNull;
    }

    return parsed;

});


