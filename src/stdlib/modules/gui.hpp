
#pragma once


// INCLUDES

#include "../util/natives.hpp"
#include "../util/gui.hpp"


// GENERAL

nFunc(gui_test, "gui", "test", {
    params({});
    string error = CaroGui::test();
    if(!error.empty()) vm->runtimeError("%s", error.c_str());
    return CaroNull;
});
