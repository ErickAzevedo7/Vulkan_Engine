#pragma once

#include "platform/Window.h"
#include <GLFW/glfw3.h>

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

        void* GetNativeHandle() const override { return m_Handle; }

        // Internal to the GLFW backend; not part of IWindow. GLFW allows
        // only one user pointer per window, so GLFWInput registers itself
        // here to receive the scroll callback.
        void _SetInputSink(GLFWInput* input) { m_UserData.InputSink = input; }

    private:
        struct UserData {
            GLFWInput* InputSink = nullptr;
        };

        static void ScrollCallback(GLFWwindow* handle, double xoffset, double yoffset);

        GLFWwindow* m_Handle = nullptr;
        UserData m_UserData;
        u32 m_Width;
        u32 m_Height;
        bool m_Resized = false;
    };

} // namespace VKE::Platform
