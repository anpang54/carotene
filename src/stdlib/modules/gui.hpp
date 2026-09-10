
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
        {ANY_NUMERIC,  false},
        {ANY_NUMERIC,  false},
    });
    auto data = std::make_unique<WindowData>();
    if(args.size() >= 1) data->window.title   = asString(args[0])->str;
    if(args.size() >= 2) data->window.width   = asNumberTo<int>(args[1]);
    if(args.size() >= 3) data->window.height  = asNumberTo<int>(args[2]);
    if(args.size() >= 4) data->window.spacing = asNumberTo<int>(args[3]);
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


// grid positions

#define GRID_POSITION {TYPE_BYTE, TYPE_UINT, TYPE_INT, TYPE_ULONG, TYPE_LONG, OBJ_ARRAY}

bool gridPosition(VM* vm, const Value& value, const char* name, int& start, int& span) {

    int first, last;
    if(isArray(value)) {
        const vector<Value>& data = asArray(value)->data;
        if(data.size() != 2 || !isInt(data[0].type) || !isInt(data[1].type)) {
            vm->runtimeError("The %s should be an integer to signify a single position, or an array of 2 integers to signify a range.", name);
            return false;
        }
        first = asNumberTo<int>(data[0]);
        last  = asNumberTo<int>(data[1]);
    } else {
        first = last = asNumberTo<int>(value);
    }

    if(first < 0 || last < first) {
        vm->runtimeError("The %s should be positive, with the first before the last.", name);
        return false;
    }

    start = first;
    span  = last - first + 1;
    return true;

}

bool addWidget(VM* vm, Value self, const vector<Value>& args, CaroGui::WidgetType type, Value callback) {

    WindowData* data = nativeData<WindowData>(vm, self);
    if(data == nullptr) return false;

    CaroGui::Widget widget{type, asString(args[2])->str};
    if(!gridPosition(vm, args[0], "x", widget.x, widget.xSpan)) return false;
    if(!gridPosition(vm, args[1], "y", widget.y, widget.ySpan)) return false;

    data->window.widgets.push_back(widget);
    data->callbacks.push_back(callback);
    return true;

}


// widgets

nMethod(gui_Window, label, {
    params({
        {GRID_POSITION, true},
        {GRID_POSITION, true},
        {{OBJ_STRING},  true}
    });
    addWidget(vm, self, args, CaroGui::WIDGET_LABEL, CaroNull);
    return CaroNull;
});

nMethod(gui_Window, button, {
    params({
        {GRID_POSITION, true},
        {GRID_POSITION, true},
        {{OBJ_STRING},  true},
        {{OBJ_FUNCTION, OBJ_NATIVE, OBJ_BOUND_METHOD}, false}
    });
    addWidget(vm, self, args, CaroGui::WIDGET_BUTTON, args.size() >= 4? args[3]: CaroNull);
    return CaroNull;
});


// TEST

nFunc(gui_test, "gui", "test", {
    params({});

    CaroGui::Window window{"Carotene test window", 400, 250, 25, {
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
            , 0, 0
        },
        {
            CaroGui::WIDGET_BUTTON,
            "Self-destruct",
            0, 1
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
