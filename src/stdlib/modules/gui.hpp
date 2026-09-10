
#pragma once


// INCLUDES

#include "../util/natives.hpp"
#include "../util/gui.hpp"


// WINDOW


// window object

struct WindowData: NativeData{
    CaroGui::Window window;
    vector<Value> callbacks;    // 1 per widget
    void mark() override {
        for(const Value& callback: callbacks) markValue(callback);
    }
};

nClass(gui_Window, "gui", "Window");


// init/show/close

nMethod(gui_Window, init, {
    params({
        {{OBJ_STRING}, false},
        {ANY_NUMERIC,  false},
        {ANY_NUMERIC,  false}
    });
    auto data = std::make_unique<WindowData>();
    if(args.size() >= 1) data->window.title  = asString(args[0])->str;
    if(args.size() >= 2) data->window.width  = asNumberTo<int>(args[1]);
    if(args.size() >= 3) data->window.height = asNumberTo<int>(args[2]);
    asInstance(self)->native = std::move(data);
    return CaroNull;
});

nMethod(gui_Window, show, {
    params({});

    WindowData* data = nativeData<WindowData>(vm, self);
    if(data == nullptr) return CaroNull;

    string error = CaroGui::show(data->window, [&](size_t widget) {
        Value callback = data->callbacks[widget];
        if(callback.type == TYPE_NULL) return true;
        Value result;
        return vm->callFromNative(callback, {}, &result);
    });

    if(vm->hadError) return CaroNull;
    if(!error.empty()) vm->runtimeError("%s", error.c_str());

    return CaroNull;
});

nMethod(gui_Window, close, {
    params({});
    CaroGui::close();
    return CaroNull;
});


// WIDGETS

nMethod(gui_Window, label, {
    params({
        {{OBJ_STRING}, true}
    });

    WindowData* data = nativeData<WindowData>(vm, self);
    if(data == nullptr) return CaroNull;
    data->window.widgets.push_back({CaroGui::WIDGET_LABEL, asString(args[0])->str});
    data->callbacks.push_back(CaroNull);
    return CaroNull;

});

nMethod(gui_Window, button, {
    params({
        {{OBJ_STRING}, true},
        {{OBJ_FUNCTION, OBJ_NATIVE, OBJ_BOUND_METHOD}, false}
    });

    WindowData* data = nativeData<WindowData>(vm, self);
    if(data == nullptr) return CaroNull;
    data->window.widgets.push_back({CaroGui::WIDGET_BUTTON, asString(args[0])->str});
    data->callbacks.push_back(args.size() >= 2? args[1]: CaroNull);
    return CaroNull;

});


// TEST

nFunc(gui_test, "gui", "test", {
    params({});

    CaroGui::Window window{"Carotene test window", 400, 250, {
        {
            CaroGui::WIDGET_LABEL,  
            #if defined(CARO_GUI_WEB)
                "Hello! This is a test webpage rendered using the DOM with Acrylic components. Pretty cool!"
            #elif defined(CARO_GUI_GTK)
                "Hello! This is a test window rendered using GTK 4. Pretty cool!"
            #elif defined(CARO_GUI_WIN32)
                "Hello! This is a test window rendered using Win32. Pretty NOT cool, this API is a mess, I can see why Microsoft is constantly trying to replace it!"
            #elif defined(CARO_GUI_BEAPI)
                "Hello! This is a test window rendered using BeAPI. Pretty cool!"
            #endif
        },
        {
            CaroGui::WIDGET_BUTTON,
            "Self-destruct"
        }
    }};

    #ifdef CARO_GUI_WEB
        const char* buttonTexts[] = {
            "Well, due to web security reasons, I can't close this tab.",
            "So go close it yourself.",
            "What?",
            "Go away!",
            "Why are you still here?",
            "Y'know what?",
            "I can't delete the tab, but I can delete myself."
        };
        size_t clicked = 0;
        auto onClick = [&](size_t) {
            if(clicked < std::size(buttonTexts)) {
                EM_ASM({
                    Module.caroContainer.querySelector("button").innerText = UTF8ToString($0);
                }, buttonTexts[clicked++]);
            } else {
                CaroGui::close();
            }
            return true;
        };
    #else
        auto onClick = [](size_t) { CaroGui::close(); return true; };
    #endif

    string error = CaroGui::show(window, onClick);
    if(!error.empty()) vm->runtimeError("%s", error.c_str());

    return CaroNull;
});
