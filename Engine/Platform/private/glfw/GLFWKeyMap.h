#pragma once

// Translation table between the engine's KeyCode/MouseButton (KeyCode.h)
// and GLFW's key/button macros.

#include "platform/KeyCode.h"
#include <GLFW/glfw3.h>

namespace VKE::Platform {

    // Returns the GLFW_KEY_* macro for a given KeyCode, or -1 if unmapped.
    inline int ToGLFWKey(KeyCode key) {
        switch (key) {
            case KeyCode::Space: return GLFW_KEY_SPACE;
            case KeyCode::Apostrophe: return GLFW_KEY_APOSTROPHE;
            case KeyCode::Comma: return GLFW_KEY_COMMA;
            case KeyCode::Minus: return GLFW_KEY_MINUS;
            case KeyCode::Period: return GLFW_KEY_PERIOD;
            case KeyCode::Slash: return GLFW_KEY_SLASH;
            case KeyCode::D0: return GLFW_KEY_0;
            case KeyCode::D1: return GLFW_KEY_1;
            case KeyCode::D2: return GLFW_KEY_2;
            case KeyCode::D3: return GLFW_KEY_3;
            case KeyCode::D4: return GLFW_KEY_4;
            case KeyCode::D5: return GLFW_KEY_5;
            case KeyCode::D6: return GLFW_KEY_6;
            case KeyCode::D7: return GLFW_KEY_7;
            case KeyCode::D8: return GLFW_KEY_8;
            case KeyCode::D9: return GLFW_KEY_9;
            case KeyCode::Semicolon: return GLFW_KEY_SEMICOLON;
            case KeyCode::Equal: return GLFW_KEY_EQUAL;
            case KeyCode::A: return GLFW_KEY_A;
            case KeyCode::B: return GLFW_KEY_B;
            case KeyCode::C: return GLFW_KEY_C;
            case KeyCode::D: return GLFW_KEY_D;
            case KeyCode::E: return GLFW_KEY_E;
            case KeyCode::F: return GLFW_KEY_F;
            case KeyCode::G: return GLFW_KEY_G;
            case KeyCode::H: return GLFW_KEY_H;
            case KeyCode::I: return GLFW_KEY_I;
            case KeyCode::J: return GLFW_KEY_J;
            case KeyCode::K: return GLFW_KEY_K;
            case KeyCode::L: return GLFW_KEY_L;
            case KeyCode::M: return GLFW_KEY_M;
            case KeyCode::N: return GLFW_KEY_N;
            case KeyCode::O: return GLFW_KEY_O;
            case KeyCode::P: return GLFW_KEY_P;
            case KeyCode::Q: return GLFW_KEY_Q;
            case KeyCode::R: return GLFW_KEY_R;
            case KeyCode::S: return GLFW_KEY_S;
            case KeyCode::T: return GLFW_KEY_T;
            case KeyCode::U: return GLFW_KEY_U;
            case KeyCode::V: return GLFW_KEY_V;
            case KeyCode::W: return GLFW_KEY_W;
            case KeyCode::X: return GLFW_KEY_X;
            case KeyCode::Y: return GLFW_KEY_Y;
            case KeyCode::Z: return GLFW_KEY_Z;
            case KeyCode::LeftBracket: return GLFW_KEY_LEFT_BRACKET;
            case KeyCode::Backslash: return GLFW_KEY_BACKSLASH;
            case KeyCode::RightBracket: return GLFW_KEY_RIGHT_BRACKET;
            case KeyCode::GraveAccent: return GLFW_KEY_GRAVE_ACCENT;
            case KeyCode::Escape: return GLFW_KEY_ESCAPE;
            case KeyCode::Enter: return GLFW_KEY_ENTER;
            case KeyCode::Tab: return GLFW_KEY_TAB;
            case KeyCode::Backspace: return GLFW_KEY_BACKSPACE;
            case KeyCode::Insert: return GLFW_KEY_INSERT;
            case KeyCode::Delete: return GLFW_KEY_DELETE;
            case KeyCode::Right: return GLFW_KEY_RIGHT;
            case KeyCode::Left: return GLFW_KEY_LEFT;
            case KeyCode::Down: return GLFW_KEY_DOWN;
            case KeyCode::Up: return GLFW_KEY_UP;
            case KeyCode::PageUp: return GLFW_KEY_PAGE_UP;
            case KeyCode::PageDown: return GLFW_KEY_PAGE_DOWN;
            case KeyCode::Home: return GLFW_KEY_HOME;
            case KeyCode::End: return GLFW_KEY_END;
            case KeyCode::CapsLock: return GLFW_KEY_CAPS_LOCK;
            case KeyCode::ScrollLock: return GLFW_KEY_SCROLL_LOCK;
            case KeyCode::NumLock: return GLFW_KEY_NUM_LOCK;
            case KeyCode::PrintScreen: return GLFW_KEY_PRINT_SCREEN;
            case KeyCode::Pause: return GLFW_KEY_PAUSE;
            case KeyCode::F1: return GLFW_KEY_F1;
            case KeyCode::F2: return GLFW_KEY_F2;
            case KeyCode::F3: return GLFW_KEY_F3;
            case KeyCode::F4: return GLFW_KEY_F4;
            case KeyCode::F5: return GLFW_KEY_F5;
            case KeyCode::F6: return GLFW_KEY_F6;
            case KeyCode::F7: return GLFW_KEY_F7;
            case KeyCode::F8: return GLFW_KEY_F8;
            case KeyCode::F9: return GLFW_KEY_F9;
            case KeyCode::F10: return GLFW_KEY_F10;
            case KeyCode::F11: return GLFW_KEY_F11;
            case KeyCode::F12: return GLFW_KEY_F12;
            case KeyCode::KP0: return GLFW_KEY_KP_0;
            case KeyCode::KP1: return GLFW_KEY_KP_1;
            case KeyCode::KP2: return GLFW_KEY_KP_2;
            case KeyCode::KP3: return GLFW_KEY_KP_3;
            case KeyCode::KP4: return GLFW_KEY_KP_4;
            case KeyCode::KP5: return GLFW_KEY_KP_5;
            case KeyCode::KP6: return GLFW_KEY_KP_6;
            case KeyCode::KP7: return GLFW_KEY_KP_7;
            case KeyCode::KP8: return GLFW_KEY_KP_8;
            case KeyCode::KP9: return GLFW_KEY_KP_9;
            case KeyCode::KPDecimal: return GLFW_KEY_KP_DECIMAL;
            case KeyCode::KPDivide: return GLFW_KEY_KP_DIVIDE;
            case KeyCode::KPMultiply: return GLFW_KEY_KP_MULTIPLY;
            case KeyCode::KPSubtract: return GLFW_KEY_KP_SUBTRACT;
            case KeyCode::KPAdd: return GLFW_KEY_KP_ADD;
            case KeyCode::KPEnter: return GLFW_KEY_KP_ENTER;
            case KeyCode::KPEqual: return GLFW_KEY_KP_EQUAL;
            case KeyCode::LeftShift: return GLFW_KEY_LEFT_SHIFT;
            case KeyCode::LeftControl: return GLFW_KEY_LEFT_CONTROL;
            case KeyCode::LeftAlt: return GLFW_KEY_LEFT_ALT;
            case KeyCode::LeftSuper: return GLFW_KEY_LEFT_SUPER;
            case KeyCode::RightShift: return GLFW_KEY_RIGHT_SHIFT;
            case KeyCode::RightControl: return GLFW_KEY_RIGHT_CONTROL;
            case KeyCode::RightAlt: return GLFW_KEY_RIGHT_ALT;
            case KeyCode::RightSuper: return GLFW_KEY_RIGHT_SUPER;
            case KeyCode::Menu: return GLFW_KEY_MENU;
            default: return -1; // GLFW_KEY_UNKNOWN
        }
    }

    inline int ToGLFWMouseButton(MouseButton button) {
        switch (button) {
            case MouseButton::Left: return GLFW_MOUSE_BUTTON_LEFT;
            case MouseButton::Right: return GLFW_MOUSE_BUTTON_RIGHT;
            case MouseButton::Middle: return GLFW_MOUSE_BUTTON_MIDDLE;
            case MouseButton::Button4: return GLFW_MOUSE_BUTTON_4;
            case MouseButton::Button5: return GLFW_MOUSE_BUTTON_5;
            case MouseButton::Button6: return GLFW_MOUSE_BUTTON_6;
            case MouseButton::Button7: return GLFW_MOUSE_BUTTON_7;
            case MouseButton::Button8: return GLFW_MOUSE_BUTTON_8;
            default: return -1;
        }
    }

} // namespace VKE::Platform
