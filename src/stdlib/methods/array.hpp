
#pragma once


// INCLUDES

#include "../util/natives.hpp"


// METHODS

#define SELF asArray(self)

nBuiltin(OBJ_ARRAY, length, {
    params({});
    return CaroUint(SELF->data.size());
});

#undef SELF
