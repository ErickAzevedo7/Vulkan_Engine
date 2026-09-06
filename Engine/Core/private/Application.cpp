#include <core/Application.h>
#include <core/Globals.h>
#include <core/Log.h>
#include <core/Assert.h>
#include <core/FrameTime.h>

namespace VKE {

    Application* Application::s_Instance = nullptr;

    Application::Application()
    {
        VKE_CORE_ASSERT(!s_Instance, "Application already exists!");
        s_Instance = this;
        VKE_CORE_INFO("Application initialized");
    }

    Application::~Application()
    {
        // Shutdown in reverse registration order
        for (auto it = m_Systems.rbegin(); it != m_Systems.rend(); ++it)
        {
            VKE_CORE_INFO("System shutdown: {}", (*it)->GetName());
            (*it)->OnShutdown();
        }
        s_Instance = nullptr;
    }

    void Application::Run()
    {
        while (m_Running)
        {
            // NOTE: real delta time comes from the platform Timer (Phase 2).
            // Fixed delta is a placeholder until Window + high-res clock are in place.
            constexpr double fixedDelta = 1.0 / 60.0;
            m_TotalTime += fixedDelta;

            FrameTime ft(fixedDelta, m_TotalTime);

            for (auto& system : m_Systems)
                system->OnUpdate(ft);

            // Prevents 100% CPU spin — remove once Window event polling exists (Phase 2).
            break;
        }
    }

    // Free function.
    // Signals the engine to stop at the end of the current frame.
    void RequestExit()
    {
        VKE_CORE_ASSERT(Application::s_Instance, "No Application instance exists!");
        Application::s_Instance->m_Running = false;
    }

} // namespace VKE
