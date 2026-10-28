#pragma once

#include <core/Types.h>
#include <string>

namespace VKE::Platform {

    // Pointer shapes the window can show while the mouse is over it.
    enum class CursorShape {
        Arrow,
        TextInput,
        Hand,
        ResizeAll,
        ResizeHorizontal,
        ResizeVertical,
        // Top-right to bottom-left.
        ResizeDiagonalUp,
        // Top-left to bottom-right.
        ResizeDiagonalDown,
        NotAllowed,
        Hidden,
    };

    struct WindowDesc {
        std::string Title = "Vulkan Engine";
        u32 Width = 1280;
        u32 Height = 720;
        bool Resizable = true;
    };

    // Owns the native window handle and pumps OS events. Backend is
    // selected at link time via IWindow::Create() (see the matching
    // backend .cpp, e.g. private/glfw/GLFWWindow.cpp). Only RHI backends
    // may interpret the pointer returned by GetNativeHandle().
    class IWindow {
    public:
        virtual ~IWindow() = default;

        static Iunique<IWindow> Create(const WindowDesc& desc);

        // Pumps OS events for this frame (resize, close, keyboard, mouse).
        // Call once per frame, before querying Input.
        virtual void PollEvents() = 0;

        // True once the OS/user has requested the window to close
        // (close button, Alt+F4, ...). Does not close the window itself.
        virtual bool ShouldClose() const = 0;

        virtual u32 GetWidth() const = 0;
        virtual u32 GetHeight() const = 0;

        // True while the window is minimized (0x0 framebuffer).
        virtual bool IsMinimized() const = 0;

        // True on the first call after the framebuffer size changed;
        // resets to false once read. Poll once per frame from the RHI
        // swapchain to know when to recreate it.
        virtual bool ConsumeResizedFlag() = 0;

        virtual void SetCursorShape(CursorShape shape) = 0;

        // Text is UTF-8. Empty when the clipboard holds no text.
        virtual std::string GetClipboardText() const = 0;
        virtual void SetClipboardText(const std::string& text) = 0;

        // Native handle for RHI surface creation (GLFWwindow* today).
        virtual void* GetNativeHandle() const = 0;
    };

} // namespace VKE::Platform
