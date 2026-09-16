#pragma once

#include <core/Types.h>
#include <rhi/RHITypes.h>

namespace VKE::Platform {
    class IWindow;
} // namespace VKE::Platform

namespace VKE::RHI {

    // Owns the graphics device. Every other RHI object is created through it.
    class RHIDevice {
    public:
        virtual ~RHIDevice() = default;

        static Iunique<RHIDevice> Create(Platform::IWindow& window);

        // Blocks until the GPU has finished all submitted work.
        virtual void WaitIdle() = 0;

        virtual const DeviceLimits& GetLimits() const = 0;

        // Best supported format for depth attachments.
        virtual Format GetDepthFormat() const = 0;
    };

} // namespace VKE::RHI
