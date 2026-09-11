
#pragma once


// INCLUDES

#include "../util/natives.hpp"


// METHODS

nBuiltin(OBJ_ARRAY, length, {
    params({});
    return CaroUint(asArray(self)->data.size());
});
