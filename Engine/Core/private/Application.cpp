#include <core/Application.h>
#include <core/Globals.h>
#include <core/Log.h>
#include <core/Assert.h>
#include <core/FrameTime.h>

#include <chrono>

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
        // Per-frame delta time, via std::chrono directly (no dependency
        // on Platform::Timer or any other module).
        auto lastTime = std::chrono::steady_clock::now();

        while (m_Running)
        {
            auto now = std::chrono::steady_clock::now();
            double delta = std::chrono::duration<double>(now - lastTime).count();
            lastTime = now;

            m_TotalTime += delta;
            FrameTime ft(delta, m_TotalTime);

            for (auto& system : m_Systems)
                system->OnUpdate(ft);
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
