#pragma once

#include <core/System.h>
#include <core/Types.h>
#include "platform/Window.h"
#include "platform/Input.h"

namespace VKE::Platform {

    // Owns the engine's Window and Input for the lifetime of the
    // Application. Populates the VKE::Window / VKE::Input globals
    // (platform/PlatformGlobals.h) in OnInitialize, clears them in
    // OnShutdown.
    class PlatformSystem : public VKE::System {
    public:
        explicit PlatformSystem(const WindowDesc& desc = {});
        ~PlatformSystem() override;

        void OnInitialize() override;
        void OnUpdate(FrameTime ft) override;
        void OnShutdown() override;

        std::string_view GetName() const override { return "PlatformSystem"; }

        IWindow& GetWindow() { return *m_Window; }
        IInput& GetInput() { return *m_Input; }

        // Whether closing the window ends the application, on by default. With it off,
        // the application reads IWindow::ShouldClose() and decides.
        void SetExitOnClose(bool exitOnClose) { m_ExitOnClose = exitOnClose; }

    private:
        WindowDesc m_Desc;
        bool m_ExitOnClose = true;
        Iunique<IWindow> m_Window;
        Iunique<IInput> m_Input;
    };

} // namespace VKE::Platform
