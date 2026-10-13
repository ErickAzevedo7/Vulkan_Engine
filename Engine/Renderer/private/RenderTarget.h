#pragma once

#include <core/Types.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHIFramebuffer.h>
#include <rhi/RHIRenderPass.h>
#include <rhi/RHITexture.h>

namespace VKE::Renderer {

    // Offscreen color and depth pair that a pass renders into, built from the
    // textures and framebuffer the RHI already provides.
    struct RenderTarget {
        Iunique<RHI::RHITexture> Color;
        Iunique<RHI::RHITexture> Depth;
        Iunique<RHI::RHIFramebuffer> Framebuffer;
        u32 Width = 0;
        u32 Height = 0;

        void Create(RHI::RHIDevice& device, RHI::RHIRenderPass& renderPass, u32 width, u32 height,
                    RHI::Format colorFormat, RHI::Format depthFormat, const char* debugName) {
            Width = width;
            Height = height;

            RHI::TextureDesc colorDesc;
            colorDesc.Width = width;
            colorDesc.Height = height;
            colorDesc.Format = colorFormat;
            colorDesc.Usage = RHI::TextureUsage::ColorAttachment | RHI::TextureUsage::Sampled;
            colorDesc.DebugName = debugName;
            Color = RHI::RHITexture::Create(device, colorDesc);

            RHI::TextureDesc depthDesc;
            depthDesc.Width = width;
            depthDesc.Height = height;
            depthDesc.Format = depthFormat;
            depthDesc.Usage = RHI::TextureUsage::DepthAttachment;
            depthDesc.DebugName = debugName;
            Depth = RHI::RHITexture::Create(device, depthDesc);

            RHI::FramebufferDesc framebufferDesc;
            framebufferDesc.AttachmentViews = {Color->GetAttachmentView(0, 0), Depth->GetAttachmentView(0, 0)};
            framebufferDesc.Width = width;
            framebufferDesc.Height = height;
            Framebuffer = RHI::RHIFramebuffer::Create(device, renderPass, framebufferDesc);
        }
    };

} // namespace VKE::Renderer
