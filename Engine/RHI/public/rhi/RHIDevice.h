#pragma once

#include <core/Types.h>

namespace VKE::Platform {
    class IWindow;
} // namespace VKE::Platform

namespace VKE::RHI {

    // Owns the graphics device for the running engine. One instance only;
    // every other RHI object is created through it, directly or indirectly.
    class RHIDevice {
    public:
        virtual ~RHIDevice() = default;

        static Iunique<RHIDevice> Create(Platform::IWindow& window);

        // Blocks until all GPU work submitted through this device has
        // completed. Call before destroying any RHI resource still in use
        // by an in-flight frame (e.g. on shutdown, or before a resize-driven
        // swapchain recreation).
        virtual void WaitIdle() = 0;
    };

} // namespace VKE::RHI
