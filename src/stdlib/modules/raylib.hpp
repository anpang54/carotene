
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


// INPUT


// helpers

const unordered_map<string, int> mapKey = {

    // top digits
    {"0", KEY_ZERO}, {"1", KEY_ONE}, {"2", KEY_TWO  }, {"3", KEY_THREE}, {"4", KEY_FOUR},
    {"5", KEY_FIVE}, {"6", KEY_SIX}, {"7", KEY_SEVEN}, {"8", KEY_EIGHT}, {"9", KEY_NINE},

    // letters
    {"a", KEY_A}, {"b", KEY_B}, {"c", KEY_C}, {"d", KEY_D}, {"e", KEY_E}, {"f", KEY_F}, {"g", KEY_G},
    {"h", KEY_H}, {"i", KEY_I}, {"j", KEY_J}, {"k", KEY_K}, {"l", KEY_L}, {"m", KEY_M}, {"n", KEY_N},
    {"o", KEY_O}, {"p", KEY_P}, {"q", KEY_Q}, {"r", KEY_R}, {"s", KEY_S}, {"t", KEY_T}, {"u", KEY_U},
    {"v", KEY_V}, {"w", KEY_W}, {"x", KEY_X}, {"y", KEY_Y}, {"z", KEY_Z},

    // punctuation
    {"`", KEY_GRAVE       },
    {"-", KEY_MINUS       }, {"=", KEY_EQUAL        },
    {"[", KEY_LEFT_BRACKET}, {"]", KEY_RIGHT_BRACKET},
    {";", KEY_SEMICOLON   }, {"'", KEY_APOSTROPHE   }, {"\\", KEY_BACKSLASH},
    {",", KEY_COMMA       }, {".", KEY_PERIOD       }, {"/" , KEY_SLASH    },

    // control
    {"escape",       KEY_ESCAPE      },
                                                                           {"backspace", KEY_BACKSPACE},
    {"tab",          KEY_TAB         },
    {"caps_lock",    KEY_CAPS_LOCK   },                                    {"enter",     KEY_ENTER    },
                                        {"space",        KEY_SPACE      },
    {"print_screen", KEY_PRINT_SCREEN}, {"scroll_lock",  KEY_SCROLL_LOCK}, {"pause",     KEY_PAUSE    },
    {"insert",       KEY_INSERT      }, {"home",         KEY_HOME       }, {"page_up",   KEY_PAGE_UP  },
    {"delete",       KEY_DELETE      }, {"end",          KEY_END        }, {"page_down", KEY_PAGE_DOWN},
                                        {"up",           KEY_UP         },
    {"left",         KEY_LEFT        }, {"down",         KEY_DOWN       }, {"right",     KEY_RIGHT    },
    {"num_lock",     KEY_NUM_LOCK    },

    // modifiers
    {"left_ctrl",  KEY_LEFT_CONTROL}, {"right_ctrl",  KEY_RIGHT_CONTROL},
    {"left_shift", KEY_LEFT_SHIFT  }, {"right_shift", KEY_RIGHT_SHIFT  },
    {"left_alt",   KEY_LEFT_ALT    }, {"right_alt",   KEY_RIGHT_ALT    },
    {"left_super", KEY_LEFT_SUPER  }, {"right_super", KEY_RIGHT_SUPER},

    // function
    {"f1", KEY_F1}, {"f2", KEY_F2}, {"f3",  KEY_F3},  {"f4",  KEY_F4 }, {"f5",  KEY_F5 }, {"f6",  KEY_F6 },
    {"f7", KEY_F7}, {"f8", KEY_F8}, {"f9",  KEY_F9},  {"f10", KEY_F10}, {"f11", KEY_F11}, {"f12", KEY_F12},

    // numpad
    {"num_0", KEY_KP_0}, {"num_1", KEY_KP_1}, {"num_2", KEY_KP_2}, {"num_3", KEY_KP_3}, {"num_4", KEY_KP_4},
    {"num_5", KEY_KP_5}, {"num_6", KEY_KP_6}, {"num_7", KEY_KP_7}, {"num_8", KEY_KP_8}, {"num_9", KEY_KP_9},
    {"num_slash", KEY_KP_DIVIDE}, {"num_asterisk", KEY_KP_MULTIPLY}, {"num_minus", KEY_KP_SUBTRACT},
                                                                     {"num_plus",  KEY_KP_ADD     },
                                  {"num_dot",      KEY_KP_DECIMAL }, {"num_enter", KEY_KP_ENTER   },

};
    // I painstakingly perfected the names and the ordering/alignment here for literally no reason

const unordered_map<string, int> mapMouse = {
    {"left",   MOUSE_BUTTON_LEFT  },
    {"middle", MOUSE_BUTTON_MIDDLE},
    {"right",  MOUSE_BUTTON_RIGHT }, 
    {"side",   MOUSE_BUTTON_SIDE  },
};

int raylibInputCode(VM* vm, const unordered_map<string, int>& codes, Value name, const char* kind) {
    const string& str = asString(name)->str;
    auto it = codes.find(lower(str));
    if(it != codes.end()) return it->second;
    vm->runtimeError("\"%s\" isn't a valid %s name.", str.c_str(), kind);
    return -1;
}

#define inputKey(cppName, caroName, raylibFunction)\
    nFunc(cppName, "raylib", caroName, {\
        params({\
            {{OBJ_STRING}, true}\
        });\
        ifWindowOpen();\
        int key = raylibInputCode(vm, mapKey, args[0], "key");\
        if(key < 0) return CaroNull;\
        return CaroBool(raylibFunction(key));\
    })

#define inputMouse(cppName, caroName, raylibFunction)\
    nFunc(cppName, "raylib", caroName, {\
        params({\
            {{OBJ_STRING}, false}\
        });\
        ifWindowOpen();\
        int button = MOUSE_BUTTON_LEFT;\
        if(args.size() >= 1) {\
            button = raylibInputCode(vm, mapMouse, args[0], "mouse button");\
            if(button < 0) return CaroNull;\
        }\
        return CaroBool(raylibFunction(button));\
    })


// actual functions

inputKey(raylib_key_down,     "key_down",     IsKeyDown);
inputKey(raylib_key_pressed,  "key_pressed",  IsKeyPressed);
inputKey(raylib_key_released, "key_released", IsKeyReleased);

inputMouse(raylib_mouse_down,     "mouse_down",     IsMouseButtonDown);
inputMouse(raylib_mouse_pressed,  "mouse_pressed",  IsMouseButtonPressed);
inputMouse(raylib_mouse_released, "mouse_released", IsMouseButtonReleased);

nFunc(raylib_mouse_position, "raylib", "mouse_position", {
    params({});
    ifWindowOpen();
    Vector2 position = GetMousePosition();
    return CaroVec2f(position.x, position.y);
});


#endif
