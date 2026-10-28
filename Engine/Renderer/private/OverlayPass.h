#pragma once

#include <core/Types.h>
#include <renderer/Overlay.h>
#include <rhi/RHIBindGroup.h>
#include <rhi/RHIBuffer.h>
#include <rhi/RHICommandBuffer.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHIPipeline.h>
#include <rhi/RHIRenderPass.h>
#include <rhi/RHISampler.h>
#include <rhi/RHITexture.h>
#include <functional>
#include <unordered_map>
#include <vector>

namespace VKE::Renderer {

    // Draws the 2D layer's lists over the window, alpha blended, with a texture and a clip
    // rectangle per command.
    class OverlayPass {
    public:
        // `outputPass` is the pass Record() is called in.
        OverlayPass(RHI::RHIDevice& device, RHI::RHIRenderPass& outputPass, u32 framesInFlight);
        ~OverlayPass();

        // The texture a command names, or null to draw it untextured.
        using TextureResolver = std::function<RHI::RHITexture*(const OverlayTexture&)>;

        // Draws `count` lists in order. `outputEncodes` is true when the attachment's format is _SRGB.
        void Record(RHI::RHICommandBuffer& cmd, u32 frameIndex, const OverlayDrawList* lists, size_t count, u32 width,
                    u32 height, bool outputEncodes, const TextureResolver& resolve);

        // Drops what the pass keeps for `texture`. Call before destroying a texture it drew,
        // once the GPU is done with it.
        void ForgetTexture(RHI::RHITexture& texture);

    private:
        struct FrameBuffers {
            Iunique<RHI::RHIBuffer> Vertices;
            Iunique<RHI::RHIBuffer> Indices;
        };

        RHI::RHIBindGroup& GetBindGroup(RHI::RHITexture& texture);
        void Upload(FrameBuffers& buffers, const OverlayDrawList* lists, size_t count, size_t vertexCount,
                    size_t indexCount);

        RHI::RHIDevice& m_Device;

        Iunique<RHI::RHIBindGroupLayout> m_Layout;
        Iunique<RHI::RHISampler> m_Sampler;
        Iunique<RHI::RHIPipeline> m_Pipeline;
        // Sampled by untextured commands.
        Iunique<RHI::RHITexture> m_White;

        std::vector<FrameBuffers> m_FrameBuffers;
        std::unordered_map<RHI::RHITexture*, Iunique<RHI::RHIBindGroup>> m_BindGroups;
        // Copies of the lists, merged into one, reused every frame.
        std::vector<OverlayVertex> m_Vertices;
        std::vector<u16> m_Indices;
    };

} // namespace VKE::Renderer
