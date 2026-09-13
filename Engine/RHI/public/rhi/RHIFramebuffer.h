#pragma once

#include <core/Types.h>
#include <rhi/RHITypes.h>

namespace VKE::RHI {

    class RHIDevice;
    class RHIRenderPass;

    // A set of attachment image views bound to a specific RHIRenderPass.
    // The sandbox creates one RHIFramebuffer per swapchain image (each
    // wrapping that image's native view from RHISwapchain::GetImageView);
    // an offscreen render target uses the same interface against a
    // render-pass-compatible RHIRenderPass.
    class RHIFramebuffer {
    public:
        virtual ~RHIFramebuffer() = default;

        static Iunique<RHIFramebuffer> Create(RHIDevice& device, RHIRenderPass& renderPass, const FramebufferDesc& desc);

        virtual u32 GetWidth() const = 0;
        virtual u32 GetHeight() const = 0;
    };

} // namespace VKE::RHI
