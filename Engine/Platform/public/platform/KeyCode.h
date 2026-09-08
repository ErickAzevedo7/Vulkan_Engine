#pragma once

#include <core/Types.h>

// Engine-owned key/mouse codes — not the same values as any windowing
// library's key enum. Each backend translates to/from its own vocabulary
// (see private/glfw/GLFWKeyMap.h for the GLFW mapping).

namespace VKE::Platform {

    enum class KeyCode : u16 {
        Unknown = 0,

        Space, Apostrophe, Comma, Minus, Period, Slash,
        D0, D1, D2, D3, D4, D5, D6, D7, D8, D9,
        Semicolon, Equal,
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        LeftBracket, Backslash, RightBracket, GraveAccent,

        Escape, Enter, Tab, Backspace, Insert, Delete,
        Right, Left, Down, Up,
        PageUp, PageDown, Home, End,
        CapsLock, ScrollLock, NumLock, PrintScreen, Pause,

        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

        KP0, KP1, KP2, KP3, KP4, KP5, KP6, KP7, KP8, KP9,
        KPDecimal, KPDivide, KPMultiply, KPSubtract, KPAdd, KPEnter, KPEqual,

        LeftShift, LeftControl, LeftAlt, LeftSuper,
        RightShift, RightControl, RightAlt, RightSuper,
        Menu,

        Count
    };

    enum class MouseButton : u8 {
        Left = 0,
        Right,
        Middle,
        Button4,
        Button5,
        Button6,
        Button7,
        Button8,

        Count
    };

} // namespace VKE::Platform
