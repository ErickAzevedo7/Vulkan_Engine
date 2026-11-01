#pragma once

#include <vke/Marks.h>

namespace vke {

    // Stays in the order of the engine's own list.
    enum class Key : unsigned short {
        Unknown = 0,

        Space, Apostrophe, Comma, Minus, Period, Slash,
        Number0, Number1, Number2, Number3, Number4, Number5, Number6, Number7, Number8, Number9,
        Semicolon, Equal,
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        LeftBracket, Backslash, RightBracket, GraveAccent,

        Escape, Enter, Tab, Backspace, Insert, Delete,
        Right, Left, Down, Up,
        PageUp, PageDown, Home, End,
        CapsLock, ScrollLock, NumLock, PrintScreen, Pause,

        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

        Pad0, Pad1, Pad2, Pad3, Pad4, Pad5, Pad6, Pad7, Pad8, Pad9,
        PadDecimal, PadDivide, PadMultiply, PadSubtract, PadAdd, PadEnter, PadEqual,

        LeftShift, LeftControl, LeftAlt, LeftSuper,
        RightShift, RightControl, RightAlt, RightSuper,
        Menu,

        Count
    };

    enum class MouseButton : unsigned char {
        Left,
        Right,
        Middle,
    };

    // The keyboard and the mouse. In the editor they only reach the game while its panel has
    // the focus.
    namespace Input {

        // True while the key is held.
        VKE_API bool IsKeyDown(Key key);
        // True only in the frame the key went down, or up.
        VKE_API bool WasKeyPressed(Key key);
        VKE_API bool WasKeyReleased(Key key);

        // The same for the mouse buttons.
        VKE_API bool IsMouseDown(MouseButton button);
        VKE_API bool WasMousePressed(MouseButton button);
        VKE_API bool WasMouseReleased(MouseButton button);

        // Where the mouse is, in pixels from the top-left corner of the game's view.
        VKE_API float GetMouseX();
        VKE_API float GetMouseY();
        // How far the wheel turned in this frame. Positive is away from the player.
        VKE_API float GetScroll();

    } // namespace Input

} // namespace vke
