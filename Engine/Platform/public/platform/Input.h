#pragma once

#include <core/Types.h>
#include "platform/KeyCode.h"

namespace VKE::Platform {

    class IWindow;

    // Polling-based input state (key/mouse/scroll), refreshed once per
    // frame by PlatformSystem::OnUpdate.
    class IInput {
    public:
        virtual ~IInput() = default;

        static Iunique<IInput> Create(IWindow& window);

        virtual bool IsKeyDown(KeyCode key) const = 0;
        virtual bool IsKeyPressed(KeyCode key) const = 0;   // true only on the frame it went down
        virtual bool IsKeyReleased(KeyCode key) const = 0;  // true only on the frame it went up

        virtual bool IsMouseButtonDown(MouseButton button) const = 0;
        virtual bool IsMouseButtonPressed(MouseButton button) const = 0;
        virtual bool IsMouseButtonReleased(MouseButton button) const = 0;

        virtual f32 GetMouseX() const = 0;
        virtual f32 GetMouseY() const = 0;

        // Accumulated scroll delta since the last Update() call.
        virtual f32 GetScrollDeltaX() const = 0;
        virtual f32 GetScrollDeltaY() const = 0;

        // Refreshes Down state and recomputes Pressed/Released against the
        // previous frame; clears the scroll accumulator. Call once per
        // frame, after the window has polled OS events.
        virtual void Update() = 0;
    };

} // namespace VKE::Platform
