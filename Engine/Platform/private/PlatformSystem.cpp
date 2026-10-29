#include "platform/PlatformSystem.h"
#include "platform/PlatformGlobals.h"

#include <core/Globals.h>
#include <core/Log.h>

namespace VKE::Platform {

    PlatformSystem::PlatformSystem(const WindowDesc& desc)
        : m_Desc(desc)
    {
    }

    PlatformSystem::~PlatformSystem() = default;

    void PlatformSystem::OnInitialize() {
        m_Window = IWindow::Create(m_Desc);
        m_Input = IInput::Create(*m_Window);

        VKE::Window = m_Window.get();
        VKE::Input = m_Input.get();

        VKE_CORE_INFO("PlatformSystem initialized");
    }

    void PlatformSystem::OnUpdate(FrameTime /*ft*/) {
        m_Window->PollEvents();
        m_Input->Update();

        if (m_ExitOnClose && m_Window->ShouldClose()) {
            VKE::RequestExit();
        }
    }

    void PlatformSystem::OnShutdown() {
        VKE::Window = nullptr;
        VKE::Input = nullptr;

        m_Input.reset();
        m_Window.reset();

        VKE_CORE_INFO("PlatformSystem shutdown");
    }

} // namespace VKE::Platform
