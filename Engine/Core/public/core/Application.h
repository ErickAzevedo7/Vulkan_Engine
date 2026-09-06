#pragma once

#include <core/Types.h>
#include <core/Log.h>
#include <core/Assert.h>
#include <core/System.h>
#include <vector>
#include <type_traits>

namespace VKE {

    class Application {
    public:
        Application();
        virtual ~Application();

        void Run();

        // Register a new system. Application takes ownership.
        // The system's OnInitialize() is called immediately.
        // Systems are ticked in registration order, shut down in reverse.
        // Usage: app.Register<RenderSystem>(arg1, arg2);
        template<typename T, typename... Args>
        T* Register(Args&&... args) {
            static_assert(std::is_base_of_v<System, T>,
                "T must derive from VKE::System");
            auto system = MakeIunique<T>(std::forward<Args>(args)...);
            T* ptr = system.get();
            VKE_CORE_INFO("System registered: {}", ptr->GetName());
            ptr->OnInitialize();
            m_Systems.push_back(std::move(system));
            return ptr;
        }

    private:
        std::vector<Iunique<System>> m_Systems;
        bool   m_Running   = true;
        double m_TotalTime = 0.0;

        // Internal singleton — not public. Used only by RequestExit().
        static Application* s_Instance;
        friend void RequestExit();
    };

    // Defined by the client (game/editor executable).
    Application* CreateApplication();

} // namespace VKE
