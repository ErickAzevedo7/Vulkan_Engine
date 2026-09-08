#include "GLFWWindow.h"
#include "GLFWInput.h"

#include <core/Log.h>
#include <core/Assert.h>

namespace VKE::Platform {

    namespace {
        u32 s_GLFWWindowCount = 0;

        void GLFWErrorCallback(int error, const char* description) {
            VKE_CORE_ERROR("GLFW error ({}): {}", error, description);
        }
    } // namespace

    GLFWWindow::GLFWWindow(const WindowDesc& desc)
        : m_Width(desc.Width), m_Height(desc.Height)
    {
        if (s_GLFWWindowCount == 0) {
            int ok = glfwInit();
            VKE_CORE_ASSERT(ok, "Failed to initialize GLFW");
            glfwSetErrorCallback(GLFWErrorCallback);
        }

        // No GL context: Vulkan owns the surface/swapchain.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, desc.Resizable ? GLFW_TRUE : GLFW_FALSE);

        m_Handle = glfwCreateWindow(static_cast<int>(desc.Width),
                                     static_cast<int>(desc.Height),
                                     desc.Title.c_str(),
                                     nullptr, nullptr);
        VKE_CORE_ASSERT(m_Handle, "Failed to create GLFW window");
        ++s_GLFWWindowCount;

        glfwSetWindowUserPointer(m_Handle, &m_UserData);
        glfwSetScrollCallback(m_Handle, &GLFWWindow::ScrollCallback);

        VKE_CORE_INFO("GLFWWindow created ({0}x{1})", desc.Width, desc.Height);
    }

    GLFWWindow::~GLFWWindow() {
        if (m_Handle) {
            glfwDestroyWindow(m_Handle);
            --s_GLFWWindowCount;
        }
        if (s_GLFWWindowCount == 0) {
            glfwTerminate();
        }
    }

    void GLFWWindow::PollEvents() {
        glfwPollEvents();

        int w, h;
        glfwGetFramebufferSize(m_Handle, &w, &h);
        if (static_cast<u32>(w) != m_Width || static_cast<u32>(h) != m_Height) {
            m_Width = static_cast<u32>(w);
            m_Height = static_cast<u32>(h);
            m_Resized = true;
        }
    }

    bool GLFWWindow::ShouldClose() const {
        return glfwWindowShouldClose(m_Handle) != 0;
    }

    bool GLFWWindow::ConsumeResizedFlag() {
        bool resized = m_Resized;
        m_Resized = false;
        return resized;
    }

    void GLFWWindow::ScrollCallback(GLFWwindow* handle, double xoffset, double yoffset) {
        auto* data = static_cast<UserData*>(glfwGetWindowUserPointer(handle));
        if (data && data->InputSink) {
            data->InputSink->_OnScroll(static_cast<f32>(xoffset), static_cast<f32>(yoffset));
        }
    }

    // Backend entry point for IWindow::Create(); see Window.h.
    Iunique<IWindow> IWindow::Create(const WindowDesc& desc) {
        return MakeIunique<GLFWWindow>(desc);
    }

} // namespace VKE::Platform
