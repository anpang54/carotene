
#pragma once


// INCLUDES

#include "../util/natives.hpp"


// MACROS

#define SELF asArray(self)


// METHODS


// getting info

nBuiltin(OBJ_ARRAY, length, {
    params({});
    return CaroUint(SELF->data.size());
});


// modifying the array

nBuiltin(OBJ_ARRAY, append, {
    params({
        {{}, true}
    });
    SELF->data.push_back(copyIfString(args[0]));
    return CaroNull;
});

nBuiltin(OBJ_ARRAY, pop, {
    params({
        {ANY_NUMERIC, false}
    });

    vector<Value>& data = SELF->data;

    if(data.empty()) {
        vm->runtimeError("That array is empty.");
        return CaroNull;
    }

    int64_t index = (int64_t)data.size() - 1;
    if(args.size() >= 1) {
        index = asNumberTo<int64_t>(args[0]);
        if(index < 0 || index >= (int64_t)data.size()) {
            vm->runtimeError("Array index out of bounds.");
            return CaroNull;
        }
    }

    Value item = data[index];
    data.erase(data.begin() + index);
    return item;

});


#undef SELF
