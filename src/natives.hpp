
#pragma once


// includes

#include "common.hpp"


// no namespace

DefineNative N_print("print", [](VM* vm, vector<Value> args) -> Value {

    p({
        {{},          true},
        {{TYPE_BOOL}, false}
    });

    if(args.size() >= 1) printValue(args[0]);
    if(args.size() < 2 || !isFalsey(args[1])) {
        cout << '\n';
    }

    return CaroNull;

});

DefineNative N_clock("clock", [](VM* vm, vector<Value> args) -> Value {
    p({});
    return CaroDouble((double)clock() / CLOCKS_PER_SEC);
});
