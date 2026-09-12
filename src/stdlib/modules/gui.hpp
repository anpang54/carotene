
#pragma once


// INCLUDES

#include "../util/natives.hpp"
#include "../util/gui.hpp"


// WIDGETS


// widget object

struct WidgetData: NativeData{
    CaroGui::Widget widget;
    Value callback = CaroNull;
    Value window   = CaroNull;
    size_t index   = 0;
    void mark() override{
        markValue(callback);
        markValue(window);
    }
};

WidgetData* widgetData(Value value) {
    return isInstance(value)? dynamic_cast<WidgetData*>(asInstance(value)->native.get()): nullptr;
}

bool callbackTakesWidget(Value callback) {
    Obj* function = callback.as.obj;
    if(function->type == OBJ_BOUND_METHOD) function = asBoundMethod(callback)->method;
    if(function->type != OBJ_FUNCTION) return false;
    return static_cast<ObjFunction*>(function)->arity >= 1;
}

void setWidgetText(VM* vm, Value self, const vector<Value>& args) {
    WidgetData* data = nativeData<WidgetData>(vm, self);
    if(data == nullptr) return;
    CaroGui::setText(data->widget, asString(args[0])->str);
}

Value replaceWidget(VM* vm, Value self, const vector<Value>& args);

Value deleteWidget(VM* vm, Value self) {
    WidgetData* data = nativeData<WidgetData>(vm, self);
    if(data == nullptr) return CaroNull;
    if(data->widget.deleted) {
        vm->runtimeError("That %s has already been deleted.", typeofValue(self).c_str());
        return CaroNull;
    }
    CaroGui::deleteWidget(data->widget);
    data->callback = CaroNull;
    data->window   = CaroNull;
    return CaroNull;
}

// every widget class gets these 2 methods as if they inherit from a Widget superclass
#define nWidgetMethods(cppClass)\
    nMethod(cppClass, delete, {\
        params({});\
        return deleteWidget(vm, self);\
    });\
    nMethod(cppClass, replace, {\
        params({\
            {{OBJ_INSTANCE}, true}\
        });\
        return replaceWidget(vm, self, args);\
    })


// gui.Label and gui.Button

struct TextWidgetData: WidgetData{

    bool readProperty(const string& name, Value& result) override{
        if(name == "text") {
            result = CaroObj(copyString(widget.text));
            return true;
        }
        return false;
    }

    bool writeProperty(const string& name, Value value, string& error) override{
        if(name == "text") {
            if(!matchesType(OBJ_STRING, value)) {
                error = format("The text should be {:s}, but {:s} was given.", typeofObjType(OBJ_STRING), typeofValue(value));
                return true;
            }
            CaroGui::setText(widget, asString(value)->str);
            return true;
        }
        return false;
    }

};


// label

nClass(gui_Label, "gui", "Label");

nMethod(gui_Label, init, {
    params({
        {{OBJ_STRING}, true}
    });
    if(alreadyInitialized(vm, self)) return CaroNull;
    auto data = std::make_unique<TextWidgetData>();
    data->widget = {CaroGui::WIDGET_LABEL, asString(args[0])->str};
    asInstance(self)->native = std::move(data);
    return CaroNull;
});

nMethod(gui_Label, set_text, {
    params({
        {{OBJ_STRING}, true}
    });
    setWidgetText(vm, self, args);
    return CaroNull;
});

nWidgetMethods(gui_Label);


// button

nClass(gui_Button, "gui", "Button");

nMethod(gui_Button, init, {
    params({
        {{OBJ_STRING}, true},
        {{OBJ_FUNCTION, OBJ_NATIVE, OBJ_BOUND_METHOD}, false}
    });
    if(alreadyInitialized(vm, self)) return CaroNull;
    auto data = std::make_unique<TextWidgetData>();
    data->widget = {CaroGui::WIDGET_BUTTON, asString(args[0])->str};
    if(args.size() >= 2) data->callback = args[1];
    asInstance(self)->native = std::move(data);
    return CaroNull;
});

nMethod(gui_Button, set_text, {
    params({
        {{OBJ_STRING}, true}
    });
    setWidgetText(vm, self, args);
    return CaroNull;
});

nWidgetMethods(gui_Button);


// textbox and textarea

struct TextInputData: WidgetData{

    bool readProperty(const string& name, Value& result) override{
        if(name == "value") {
            result = CaroObj(copyString(widget.text));
            return true;
        }
        return false;
    }

    bool writeProperty(const string& name, Value value, string& error) override{
        if(name == "value") {
            if(!matchesType(OBJ_STRING, value)) {
                error = format("The value should be {:s}, but {:s} was given.", typeofObjType(OBJ_STRING), typeofValue(value));
                return true;
            }
            CaroGui::setText(widget, asString(value)->str);
            return true;
        }
        return false;
    }

};

Value initTextInput(VM* vm, Value self, const vector<Value>& args, CaroGui::WidgetType type) {
    if(alreadyInitialized(vm, self)) return CaroNull;
    auto data = std::make_unique<TextInputData>();
    data->widget = {type, args.size() >= 1? asString(args[0])->str: ""};
    asInstance(self)->native = std::move(data);
    return CaroNull;
}

nClass(gui_Textbox, "gui", "Textbox");

nMethod(gui_Textbox, init, {
    params({
        {{OBJ_STRING}, false}
    });
    return initTextInput(vm, self, args, CaroGui::WIDGET_TEXTBOX);
});

nWidgetMethods(gui_Textbox);

nClass(gui_Textarea, "gui", "Textarea");

nMethod(gui_Textarea, init, {
    params({
        {{OBJ_STRING}, false}
    });
    return initTextInput(vm, self, args, CaroGui::WIDGET_TEXTAREA);
});

nWidgetMethods(gui_Textarea);


// select and combobox

bool widgetOptions(VM* vm, Value array, const char* widgetName, vector<string>& options) {
    for(const Value& option: asArray(array)->data) {
        if(!matchesType(OBJ_STRING, option)) {
            vm->runtimeError("The options should be %s, but there's %s.", typeofObjType(OBJ_STRING).c_str(), typeofValue(option).c_str());
            return false;
        }
        options.push_back(asString(option)->str);
    }
    if(options.empty()) {
        vm->runtimeError("A %s needs at least 1 option.", widgetName);
        return false;
    }
    return true;
}

struct SelectData: WidgetData{

    bool readProperty(const string& name, Value& result) override {
        if(name == "value") {
            result = CaroObj(copyString(widget.options[widget.selected]));
            return true;
        }
        return false;
    }

    bool writeProperty(const string& name, Value value, string& error) override {
        if(name == "value") {
            if(!matchesType(OBJ_STRING, value)) {
                error = format("The value should be {:s}, but {:s} was given.", typeofObjType(OBJ_STRING), typeofValue(value));
                return true;
            }
            auto found = std::ranges::find(widget.options, asString(value)->str);
            if(found == widget.options.end()) {
                error = format("\"{:s}\" isn't an option.", asString(value)->str);
                return true;
            }
            CaroGui::setSelected(widget, (int)(found - widget.options.begin()));
            return true;
        }
        return false;
    }

};

nClass(gui_Select, "gui", "Select");

nMethod(gui_Select, init, {
    params({
        {{OBJ_ARRAY}, true},
        {{OBJ_FUNCTION, OBJ_NATIVE, OBJ_BOUND_METHOD}, false}
    });
    if(alreadyInitialized(vm, self)) return CaroNull;

    vector<string> options;
    if(!widgetOptions(vm, args[0], "select", options)) return CaroNull;

    auto data = std::make_unique<SelectData>();
    data->widget = {CaroGui::WIDGET_SELECT, ""};
    data->widget.options = std::move(options);
    if(args.size() >= 2) data->callback = args[1];
    asInstance(self)->native = std::move(data);

    return CaroNull;
});

nWidgetMethods(gui_Select);

nClass(gui_ComboBox, "gui", "ComboBox");

nMethod(gui_ComboBox, init, {
    params({
        {{OBJ_ARRAY}, true},
        {{OBJ_FUNCTION, OBJ_NATIVE, OBJ_BOUND_METHOD}, false}
    });
    if(alreadyInitialized(vm, self)) return CaroNull;

    vector<string> options;
    if(!widgetOptions(vm, args[0], "combobox", options)) return CaroNull;

    auto data = std::make_unique<TextInputData>();
    data->widget = {CaroGui::WIDGET_COMBOBOX, ""};
    data->widget.options = std::move(options);
    if(args.size() >= 2) data->callback = args[1];
    asInstance(self)->native = std::move(data);

    return CaroNull;
});

nWidgetMethods(gui_ComboBox);


// WINDOW


// window object

struct WindowData: NativeData{
    CaroGui::Window window;
    vector<Value> widgets;
    bool shown = false;
    void mark() override {
        for(const Value& widget: widgets) markValue(widget);
    }
};

WindowData* windowData(Value value) {
    return isInstance(value)? dynamic_cast<WindowData*>(asInstance(value)->native.get()): nullptr;
}

nClass(gui_Window, "gui", "Window");


// init/show/close

nMethod(gui_Window, init, {
    params({
        {{OBJ_STRING}, false},
        {ANY_NUMERIC,  false},
        {ANY_NUMERIC,  false},
        {ANY_NUMERIC,  false},
    });
    if(alreadyInitialized(vm, self)) return CaroNull;
    auto data = std::make_unique<WindowData>();
    if(args.size() >= 1) data->window.title   = asString(args[0])->str;
    if(args.size() >= 2) data->window.width   = asNumberTo<int>(args[1]);
    if(args.size() >= 3) data->window.height  = asNumberTo<int>(args[2]);
    if(args.size() >= 4) data->window.spacing = asNumberTo<int>(args[3]);
    asInstance(self)->native = std::move(data);
    return CaroNull;
});

nMethod(gui_Window, show, {
    params({
        {{OBJ_FUNCTION, OBJ_NATIVE, OBJ_BOUND_METHOD}, false}
    });

    WindowData* data = nativeData<WindowData>(vm, self);
    if(data == nullptr) return CaroNull;

    // optional callback that immediately runs once the window is open
    CaroGui::ShowHandler onShow;
    if(args.size() >= 1) {
        Value callback = args[0];
        onShow = [vm, callback]() {
            Value result;
            return vm->callFromNative(callback, {}, &result);
        };
    }

    data->shown = true;
    string error = CaroGui::show(data->window, [&](size_t widget) {
        Value self = data->widgets[widget];
        WidgetData* clicked = widgetData(self);
        if(clicked == nullptr || clicked->widget.deleted) return true;
        Value callback = clicked->callback;
        if(callback.type == TYPE_NULL) return true;
        Value result;
        vector<Value> args;
        if(callbackTakesWidget(callback)) args.push_back(self);
        return vm->callFromNative(callback, args, &result);
    }, onShow);
    data->shown = false;

    if(vm->hadError) return CaroNull;
    if(!error.empty()) vm->runtimeError("%s", error.c_str());

    return CaroNull;
});

nMethod(gui_Window, close, {
    params({});
    CaroGui::close();
    return CaroNull;
});


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


// adding widgets

nMethod(gui_Window, add, {
    params({
        {{OBJ_INSTANCE}, true},
        {GRID_POSITION,  true},
        {GRID_POSITION,  true}
    });

    WindowData* data = nativeData<WindowData>(vm, self);
    if(data == nullptr) return CaroNull;

    // check widget
    WidgetData* widget = widgetData(args[0]);
    if(widget == nullptr) {
        vm->runtimeError("%s is not a widget.", typeofValue(args[0]).c_str());
        return CaroNull;
    }
    if(widget->window.type != TYPE_NULL) {
        vm->runtimeError("That %s is already in a window.", typeofValue(args[0]).c_str());
        return CaroNull;
    }
    if(widget->widget.deleted) {
        vm->runtimeError("That %s has been deleted.", typeofValue(args[0]).c_str());
        return CaroNull;
    }
    if(data->shown) {
        vm->runtimeError("Widgets can't be added while the window is open.");
        return CaroNull;
    }

    // check positions
    CaroGui::Widget& w = widget->widget;
    if(!gridPosition(vm, args[1], "x", w.x, w.xSpan)) return CaroNull;
    if(!gridPosition(vm, args[2], "y", w.y, w.ySpan)) return CaroNull;

    // add
    widget->window = self;
    widget->index  = data->widgets.size();
    data->window.widgets.push_back(&w);
    data->widgets.push_back(args[0]);

    return CaroNull;
});


// replacing widgets

Value replaceWidget(VM* vm, Value self, const vector<Value>& args) {

    WidgetData* data = nativeData<WidgetData>(vm, self);
    if(data == nullptr) return CaroNull;

    // check the widget being replaced
    if(data->widget.deleted) {
        vm->runtimeError("That %s has been deleted.", typeofValue(self).c_str());
        return CaroNull;
    }
    if(data->window.type == TYPE_NULL) {
        vm->runtimeError("That %s isn't in a window.", typeofValue(self).c_str());
        return CaroNull;
    }

    // check the widget replacing it
    WidgetData* with = widgetData(args[0]);
    if(with == nullptr) {
        vm->runtimeError("%s isn't a widget.", typeofValue(args[0]).c_str());
        return CaroNull;
    }
    if(with->widget.deleted) {
        vm->runtimeError("That %s has been deleted.", typeofValue(args[0]).c_str());
        return CaroNull;
    }
    if(with->window.type != TYPE_NULL) {
        vm->runtimeError("That %s is already in a window.", typeofValue(args[0]).c_str());
        return CaroNull;
    }

    // get window, position, and index
    WindowData* window = windowData(data->window);
    if(window == nullptr) return CaroNull;
    CaroGui::Widget& w = with->widget;
    w.x     = data->widget.x;
    w.y     = data->widget.y;
    w.xSpan = data->widget.xSpan;
    w.ySpan = data->widget.ySpan;
    size_t index = data->index;

    // replace
    CaroGui::replaceWidget(data->widget, w, index);

    // bookkeeping
    with->window   = data->window;
    with->index    = index;
    data->window   = CaroNull;
    data->callback = CaroNull;
    window->window.widgets[index] = &w;
    window->widgets[index]        = args[0];

    return CaroNull;

}


// TEST

nFunc(gui_test, "gui", "test", {
    params({});

    CaroGui::Widget label{
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
    };
    CaroGui::Widget button{CaroGui::WIDGET_BUTTON, "Self-destruct", 0, 1};

    CaroGui::Window window{"Carotene test window", 400, 250, 25, {&label, &button}};

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
                CaroGui::setText(button, buttonTexts[clicked++]);
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
