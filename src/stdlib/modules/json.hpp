

#pragma once


// INCLUDES

#include "../util/natives.hpp"
#include "../../core/value.hpp"


// FUNCTIONS

nFunc(json_stringify, "json", "stringify", {
    params({
        {{}, true}
    });
    return CaroObj(copyString(printValue(args[0], true)));
});

// json.stringify() is just a few modifications to printValue(), but json.parse() will have to be completely custom
// currently print(json.stringify([1, 2, 3])) prints nothing because print() treats [1,2,3] as a string formatting tag
