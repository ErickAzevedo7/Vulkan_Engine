#pragma once

#include "platform/Window.h"
#include <GLFW/glfw3.h>
#include <array>

namespace VKE::Platform {

    class GLFWInput;

    class GLFWWindow : public IWindow {
    public:
        explicit GLFWWindow(const WindowDesc& desc);
        ~GLFWWindow() override;

        void PollEvents() override;
        bool ShouldClose() const override;

        u32 GetWidth() const override { return m_Width; }
        u32 GetHeight() const override { return m_Height; }
        bool IsMinimized() const override { return m_Width == 0 || m_Height == 0; }

        bool ConsumeResizedFlag() override;

        void SetCursorShape(CursorShape shape) override;
        std::string GetClipboardText() const override;
        void SetClipboardText(const std::string& text) override;

        void* GetNativeHandle() const override { return m_Handle; }

        // Internal to the GLFW backend; not part of IWindow. GLFW allows
        // only one user pointer per window, so GLFWInput registers itself
        // here to receive the scroll and character callbacks.
        void _SetInputSink(GLFWInput* input) { m_UserData.InputSink = input; }

    private:
        struct UserData {
            GLFWInput* InputSink = nullptr;
        };

        static void ScrollCallback(GLFWwindow* handle, double xoffset, double yoffset);
        static void CharCallback(GLFWwindow* handle, unsigned int codepoint);

        GLFWwindow* m_Handle = nullptr;
        UserData m_UserData;
        u32 m_Width;
        u32 m_Height;
        bool m_Resized = false;
        // Created the first time each shape is shown. Null for the arrow, and where the
        // system has no such cursor.
        std::array<GLFWcursor*, static_cast<size_t>(CursorShape::Hidden)> m_Cursors{};
        CursorShape m_CursorShape = CursorShape::Arrow;
    };

} // namespace VKE::Platform
