#pragma once

#include "platform/Input.h"
#include <array>

struct GLFWwindow;

namespace VKE::Platform {

    class GLFWWindow;

    class GLFWInput : public IInput {
    public:
        explicit GLFWInput(GLFWWindow& window);

        bool IsKeyDown(KeyCode key) const override;
        bool IsKeyPressed(KeyCode key) const override;
        bool IsKeyReleased(KeyCode key) const override;

        bool IsMouseButtonDown(MouseButton button) const override;
        bool IsMouseButtonPressed(MouseButton button) const override;
        bool IsMouseButtonReleased(MouseButton button) const override;

        f32 GetMouseX() const override { return m_MouseX; }
        f32 GetMouseY() const override { return m_MouseY; }
        f32 GetScrollDeltaX() const override { return m_ScrollDeltaX; }
        f32 GetScrollDeltaY() const override { return m_ScrollDeltaY; }

        void Update() override;

        // Internal to the GLFW backend; invoked by GLFWWindow's scroll
        // callback. Not part of IInput.
        void _OnScroll(f32 xoffset, f32 yoffset);

    private:
        static constexpr size_t kKeyCount = static_cast<size_t>(KeyCode::Count);
        static constexpr size_t kButtonCount = static_cast<size_t>(MouseButton::Count);

        GLFWwindow* m_Handle;

        std::array<bool, kKeyCount> m_CurrentKeys{};
        std::array<bool, kKeyCount> m_PreviousKeys{};
        std::array<bool, kButtonCount> m_CurrentButtons{};
        std::array<bool, kButtonCount> m_PreviousButtons{};

        f32 m_MouseX = 0.0f;
        f32 m_MouseY = 0.0f;
        f32 m_ScrollDeltaX = 0.0f;
        f32 m_ScrollDeltaY = 0.0f;
        f32 m_ScrollAccumX = 0.0f;
        f32 m_ScrollAccumY = 0.0f;
    };

} // namespace VKE::Platform
