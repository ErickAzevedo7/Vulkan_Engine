#include "GLFWInput.h"
#include "GLFWWindow.h"
#include "GLFWKeyMap.h"

#include <GLFW/glfw3.h>

namespace VKE::Platform {

    GLFWInput::GLFWInput(GLFWWindow& window)
        : m_Handle(static_cast<GLFWwindow*>(window.GetNativeHandle()))
    {
        window._SetInputSink(this);
    }

    bool GLFWInput::IsKeyDown(KeyCode key) const {
        return m_CurrentKeys[static_cast<size_t>(key)];
    }

    bool GLFWInput::IsKeyPressed(KeyCode key) const {
        size_t i = static_cast<size_t>(key);
        return m_CurrentKeys[i] && !m_PreviousKeys[i];
    }

    bool GLFWInput::IsKeyReleased(KeyCode key) const {
        size_t i = static_cast<size_t>(key);
        return !m_CurrentKeys[i] && m_PreviousKeys[i];
    }

    bool GLFWInput::IsMouseButtonDown(MouseButton button) const {
        return m_CurrentButtons[static_cast<size_t>(button)];
    }

    bool GLFWInput::IsMouseButtonPressed(MouseButton button) const {
        size_t i = static_cast<size_t>(button);
        return m_CurrentButtons[i] && !m_PreviousButtons[i];
    }

    bool GLFWInput::IsMouseButtonReleased(MouseButton button) const {
        size_t i = static_cast<size_t>(button);
        return !m_CurrentButtons[i] && m_PreviousButtons[i];
    }

    void GLFWInput::Update() {
        m_PreviousKeys = m_CurrentKeys;
        for (size_t i = 0; i < kKeyCount; ++i) {
            int glfwKey = ToGLFWKey(static_cast<KeyCode>(i));
            m_CurrentKeys[i] = (glfwKey >= 0) && (glfwGetKey(m_Handle, glfwKey) == GLFW_PRESS);
        }

        m_PreviousButtons = m_CurrentButtons;
        for (size_t i = 0; i < kButtonCount; ++i) {
            int glfwButton = ToGLFWMouseButton(static_cast<MouseButton>(i));
            m_CurrentButtons[i] = (glfwButton >= 0) && (glfwGetMouseButton(m_Handle, glfwButton) == GLFW_PRESS);
        }

        double x, y;
        glfwGetCursorPos(m_Handle, &x, &y);
        m_MouseX = static_cast<f32>(x);
        m_MouseY = static_cast<f32>(y);

        m_ScrollDeltaX = m_ScrollAccumX;
        m_ScrollDeltaY = m_ScrollAccumY;
        m_ScrollAccumX = 0.0f;
        m_ScrollAccumY = 0.0f;

        m_TypedCharacters.swap(m_PendingCharacters);
        m_PendingCharacters.clear();
    }

    void GLFWInput::_OnScroll(f32 xoffset, f32 yoffset) {
        m_ScrollAccumX += xoffset;
        m_ScrollAccumY += yoffset;
    }

    void GLFWInput::_OnCharacter(u32 codepoint) {
        m_PendingCharacters.push_back(codepoint);
    }

    // Backend entry point for IInput::Create(); see Input.h. Safe to
    // static_cast: IWindow::Create() in this backend only ever constructs
    // a GLFWWindow.
    Iunique<IInput> IInput::Create(IWindow& window) {
        return MakeIunique<GLFWInput>(static_cast<GLFWWindow&>(window));
    }

} // namespace VKE::Platform
