
#pragma once
#ifdef CARO_RAYLIB


// INCLUDES

#include "../util/natives.hpp"
#include "../util/raylib.hpp"


// WINDOWS

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


// init/show

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

nMethod(raylib_Window, show, {
    params({});

    // get data
    RaylibWindowData* data = nativeData<RaylibWindowData>(vm, self);
    if(data == nullptr) return CaroNull;

    // check if there's already a window
    if(IsWindowReady()) {
        vm->runtimeError("A raylib window is already open.");
        return CaroNull;
    }
        // todo: maybe circumvent this in the future by using multiple processes

    // set config
    SetTraceLogLevel(LOG_WARNING);      // don't log literally everything
    SetConfigFlags(FLAG_VSYNC_HINT);    // better than hardcoding the fps to 60 or smth

    #ifdef __EMSCRIPTEN__
        // make a <canvas> for web
        EM_ASM({
            if(!Module.canvas) {
                const canvas = document.createElement("canvas");
                canvas.id = "caro-raylib";
                canvas.oncontextmenu = (event) => event.preventDefault();
                document.body.appendChild(canvas);
                Module.canvas = canvas;
            } else if(!Module.canvas.id) {
                Module.canvas.id = "caro-raylib";
            }
        });
    #endif

    // make window
    InitWindow(data->width, data->height, data->title.c_str());
    if(!IsWindowReady()) {
        vm->runtimeError("Couldn't open the window.");
        return CaroNull;
    }
    data->shown = true;

    const char* text = "rey lyp";
    const int size = 20;

    // loop
    while(!WindowShouldClose()) {

        BeginDrawing();

        ClearBackground(RAYWHITE);

        int width = MeasureText(text, size);
        rlDrawText(text, (GetScreenWidth() - width) / 2, (GetScreenHeight() - size) / 2, size, BLACK);

        EndDrawing();

    }

    // close
    rlCloseWindow();
    data->shown = false;

    return CaroNull;
});

#endif
