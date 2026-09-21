#pragma once

#include <core/Types.h>
#include <rhi/RHITypes.h>
#include <functional>

namespace VKE::Platform {
    class IWindow;
} // namespace VKE::Platform

namespace VKE::RHI {

    class RHICommandBuffer;

    // Owns the graphics device. Every other RHI object is created through it.
    class RHIDevice {
    public:
        virtual ~RHIDevice() = default;

        static Iunique<RHIDevice> Create(Platform::IWindow& window);

        // Blocks until the GPU has finished all submitted work.
        virtual void WaitIdle() = 0;

        // Records and runs one command buffer outside the frame loop, blocking
        // until the GPU is done. Used for setup work such as baking textures.
        virtual void ExecuteImmediate(const std::function<void(RHICommandBuffer&)>& record) = 0;

        virtual const DeviceLimits& GetLimits() const = 0;

        // Best supported format for depth attachments.
        virtual Format GetDepthFormat() const = 0;
    };

} // namespace VKE::RHI
