
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


// helpers

Color raylibColor(Value value) {
    return Color{value.as.Acolor.r, value.as.Acolor.g, value.as.Acolor.b, value.as.Acolor.a};
}

Vector2 raylibVector2(Value value) {
    return {asNumberTo<float>(getComponent(value, 0)), asNumberTo<float>(getComponent(value, 1))};
}
Vector3 raylibVector3(Value value) {
    return {asNumberTo<float>(getComponent(value, 0)), asNumberTo<float>(getComponent(value, 1)), asNumberTo<float>(getComponent(value, 2))};
}


// DRAWING

#define ifWindowOpen() if(!raylibWindowOpen(vm)) return CaroNull;

bool raylibWindowOpen(VM* vm) {
    if(IsWindowReady()) return true;
    vm->runtimeError("No raylib window is open.");
    return false;
}


// begin/end

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

nFunc(raylib_begin_3d, "raylib", "begin_3d", {
    params({
        {ANY_VEC3,    true },    // camera position
        {ANY_VEC3,    true },    // camera rotation, instead of raylib's camera target
        {ANY_NUMERIC, false},    // vertical fov
    });
    ifWindowOpen();

    Vector3 position = raylibVector3(args[0]);
    Vector3 rotation = raylibVector3(args[1]);

    // compute camera target from rotation with meth
    float pitch = rotation.x * DEG2RAD;
    float yaw   = rotation.y * DEG2RAD;
    float roll  = rotation.z * DEG2RAD;
    float sp = std::sin(pitch), cp = std::cos(pitch);
    float sy = std::sin(yaw),   cy = std::cos(yaw);
    float sr = std::sin(roll),  cr = std::cos(roll);
    Vector3 forward = {-sy * cp, sp,   -cy * cp};
    Vector3 right   = { cy,      0.0f, -sy     };
    Vector3 up      = { sy * sp, cp,    cy * sp};

    // make camera
    Camera3D camera = {
        .position   = position,
        .target     = {position.x + forward.x, position.y + forward.y, position.z + forward.z},
        .up         = {up.x * cr + right.x * sr, up.y * cr + right.y * sr, up.z * cr + right.z * sr},
        .fovy       = args.size() >= 3? asNumberTo<float>(args[2]): 66.66f,
        .projection = CAMERA_PERSPECTIVE
    };

    // start 3d mode
    BeginMode3D(camera);

    return CaroNull;
});

nFunc(raylib_end_3d, "raylib", "end_3d", {
    params({});
    ifWindowOpen();
    EndMode3D();
    return CaroNull;
});


// background

nFunc(raylib_background, "raylib", "background", {
    params({
        {{TYPE_COLOR}, true}
    });
    ifWindowOpen();
    Color color = raylibColor(args[0]);
    ClearBackground(color);
    return CaroNull;
});


// 2d shapes

nFunc(raylib_rectangle, "raylib", "rectangle", {
    params({
        {ANY_VEC2,     true},    // position
        {ANY_VEC2,     true},    // size
        {{TYPE_COLOR}, true}     // color
    });
    ifWindowOpen();
    Color color = raylibColor(args[2]);
    DrawRectangleV(raylibVector2(args[0]), raylibVector2(args[1]), color);
    return CaroNull;
});

nFunc(raylib_rectangle_outline, "raylib", "rectangle_outline", {
    params({
        {ANY_VEC2,     true },    // position
        {ANY_VEC2,     true },    // size
        {{TYPE_COLOR}, true },    // color
        {ANY_NUMERIC,  false}     // thickness
    });
    ifWindowOpen();
    Color   color    = raylibColor(args[2]);
    Vector2 position = raylibVector2(args[0]);
    Vector2 size     = raylibVector2(args[1]);
    float thickness  = args.size() >= 4? asNumberTo<float>(args[3]): 1.0f;
    DrawRectangleLinesEx({position.x, position.y, size.x, size.y}, thickness, color);
    return CaroNull;
});

nFunc(raylib_text, "raylib", "text", {
    params({
        {{OBJ_STRING}, true },    // text
        {ANY_VEC2,     true },    // position
        {{TYPE_COLOR}, true },    // color
        {ANY_NUMERIC,  false}     // font size
    });
    ifWindowOpen();
    Color color      = raylibColor(args[2]);
    Vector2 position = raylibVector2(args[1]);
    int fontSize     = args.size() >= 4? asNumberTo<int>(args[3]): 16;
    rlDrawText(asString(args[0])->str.c_str(), (int)position.x, (int)position.y, fontSize, color);
    return CaroNull;
});


// 3d shapes

nFunc(raylib_cube, "raylib", "cube", {
    params({
        {ANY_VEC3,     true},    // center
        {ANY_VEC3,     true},    // size
        {{TYPE_COLOR}, true}     // color
    });
    ifWindowOpen();
    Color color = raylibColor(args[2]);
    DrawCubeV(raylibVector3(args[0]), raylibVector3(args[1]), color);
    return CaroNull;
});

nFunc(raylib_cube_outline, "raylib", "cube_outline", {
    params({
        {ANY_VEC3,     true},    // center
        {ANY_VEC3,     true},    // size
        {{TYPE_COLOR}, true}     // color
    });
    ifWindowOpen();
    Color color = raylibColor(args[2]);
    DrawCubeWiresV(raylibVector3(args[0]), raylibVector3(args[1]), color);
    return CaroNull;
});

nFunc(raylib_plane, "raylib", "plane", {
    params({
        {ANY_VEC3,     true},    // center
        {ANY_VEC2,     true},    // horizontal size
        {{TYPE_COLOR}, true}     // color
    });
    ifWindowOpen();
    Color color = raylibColor(args[2]);
    DrawPlane(raylibVector3(args[0]), raylibVector2(args[1]), color);
    return CaroNull;
});


#endif
