#pragma once

#include <core/FrameTime.h>
#include <string_view>

namespace VKE {

    // Base class for all engine systems.
    //
    // Lifecycle (called by Application in order):
    //   OnInitialize() → OnUpdate() [every frame] → OnShutdown()
    //
    // Systems are shut down in reverse registration order.
    class System {
    public:
        virtual ~System() = default;

        // Called once when the system is registered with the Application.
        virtual void OnInitialize() {}

        // Called once per frame, in registration order.
        virtual void OnUpdate(FrameTime ft) {}

        // Called once on Application shutdown, in reverse registration order.
        virtual void OnShutdown() {}

        // Human-readable name used for logging and debug tools.
        virtual std::string_view GetName() const = 0;
    };

} // namespace VKE
