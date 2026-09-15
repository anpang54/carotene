
#pragma once

#if defined(CARO_RAYLIB)

    // a bunch of preprocessor directives to stop name conflicts between raylib and other libraries

    #undef KEY_UP
    #undef KEY_DOWN
    #undef KEY_LEFT
    #undef KEY_RIGHT
    #undef KEY_HOME
    #undef KEY_END
    #undef KEY_ENTER
    #undef KEY_TAB
    #undef KEY_SPACE
    #undef KEY_F
    #undef KEY_F1
    #undef KEY_F2
    #undef KEY_F3
    #undef KEY_F4
    #undef KEY_F5
    #undef KEY_F6
    #undef KEY_F7
    #undef KEY_F8
    #undef KEY_F9
    #undef KEY_F10
    #undef KEY_F11
    #undef KEY_F12

    #pragma push_macro("LoadImage")
    #pragma push_macro("DrawText")
    #pragma push_macro("DrawTextEx")
    #pragma push_macro("PlaySound")

    #define Rectangle   rlRectangle
    #define CloseWindow rlCloseWindow
    #define ShowCursor  rlShowCursor
    #define LoadImage   rlLoadImage
    #define DrawText    rlDrawText
    #define DrawTextEx  rlDrawTextEx
    #define PlaySound   rlPlaySound

    // actually include raylib
    #include "../../../include/raylib/raylib.h"
    #include "../../../include/raylib/rcamera.h"

    #undef Rectangle
    #undef CloseWindow
    #undef ShowCursor

    #pragma pop_macro("LoadImage")
    #pragma pop_macro("DrawText")
    #pragma pop_macro("DrawTextEx")
    #pragma pop_macro("PlaySound")

    // also lunasvg
    #include "../../../include/lunasvg/include/lunasvg.h"

#endif
