
#pragma once
#ifdef CARO_RAYLIB


// INCLUDES

#include "../util/natives.hpp"
#include "../util/raylib.hpp"


// WINDOW OBJECT

// basically the same design as gui.Window

struct RaylibWindowData: NativeData{
    string title;
    int width;
    int height;
    bool shown = false;
};

RaylibWindowData* raylibWindowData(Value value) {
    return isInstance(value)? dynamic_cast<RaylibWindowData*>(asInstance(value)->native.get()): nullptr;
}

nClass(raylib_Window, "raylib", "Window");
    // todo: consider renaming to raylib.Game


// constructor

nMethod(raylib_Window, init, {
    params({
        {{OBJ_STRING}, false},
        {ANY_NUMERIC,  false},
        {ANY_NUMERIC,  false}
    });
    if(alreadyInitialized(vm, self)) return CaroNull;

    // get data
    string title = args.size() >= 1? asString(args[0])->str: "Carotene";
    int width    = args.size() >= 2? asNumberTo<int>(args[1]): 800;
    int height   = args.size() >= 3? asNumberTo<int>(args[2]): 450;
        // 800x450 is seemingly the default raylib window size
        // on web the canvas is just made fullscreen anyway
    if(width <= 0 || height <= 0) {
        vm->runtimeError("The window's width and height must be positive.");
        return CaroNull;
    }

    // set stuff
    auto data = std::make_unique<RaylibWindowData>();
    data->title  = title;
    data->width  = width;
    data->height = height;
    asInstance(self)->native = std::move(data);

    return CaroNull;
});


// state

#define getWindowData()\
    RaylibWindowData* data = nativeData<RaylibWindowData>(vm, self);\
    if(data == nullptr) return CaroNull;

nMethod(raylib_Window, show, {
    params({});
    getWindowData();

    // check if there's already a window
    if(IsWindowReady()) {
        vm->runtimeError("A raylib window is already open.");
        return CaroNull;
    }
        // todo: maybe circumvent this in the future by using multiple processes

    // set config
    SetTraceLogLevel(LOG_WARNING);           // don't log literally everything
    unsigned int flags = FLAG_VSYNC_HINT;    // better than hardcoding the fps to 60 or smth

    #ifdef __EMSCRIPTEN__
        // make a <canvas> for web
        bool created = EM_ASM_INT({
            if(Module.canvas) {
                if(!Module.canvas.id) Module.canvas.id = "caro-raylib";
                return 0;
            }
            const canvas = document.createElement("canvas");
            canvas.id             = "caro-raylib";
            canvas.style.display  = "block";
            canvas.style.position = "fixed";
            canvas.style.inset    = 0;
            canvas.oncontextmenu  = (event) => event.preventDefault();
            document.body.appendChild(canvas);
            Module.canvas = canvas;
            return 1;
        }) != 0;
        if(created) flags |= FLAG_WINDOW_RESIZABLE;
    #endif

    SetConfigFlags(flags);

    // make window
    InitWindow(data->width, data->height, data->title.c_str());
    if(!IsWindowReady()) {
        vm->runtimeError("Couldn't open the window.");
        return CaroNull;
    }
    data->shown = true;

    return CaroNull;
});

nMethod(raylib_Window, running, {
    params({});
    getWindowData();

    return CaroBool(data->shown && !WindowShouldClose());

    // I just don't like how raylib's while loop condition is negated
    // like why say "not supposed to close", just say "supposed to run"
    // like yeah they're the same thing and both trivial but why?

});

nMethod(raylib_Window, close, {
    params({});
    getWindowData();

    if(data->shown) {
        rlCloseWindow();
        data->shown = false;
    }

    return CaroNull;
});


// DRAWING

#define ifWindowOpen() if(!raylibWindowOpen(vm)) return CaroNull;

bool raylibWindowOpen(VM* vm) {
    if(IsWindowReady()) return true;
    vm->runtimeError("No raylib window is open.");
    return false;
}

nFunc(raylib_begin, "raylib", "begin", {
    params({});
    ifWindowOpen();
    BeginDrawing();
    return CaroNull;
});

nFunc(raylib_end, "raylib", "end", {
    params({});
    ifWindowOpen();
    EndDrawing();
    return CaroNull;
});


#endif
