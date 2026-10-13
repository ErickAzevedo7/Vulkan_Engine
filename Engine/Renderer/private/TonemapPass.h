#pragma once

#include <core/Types.h>
#include <rhi/RHIBindGroup.h>
#include <rhi/RHICommandBuffer.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHIPipeline.h>
#include <rhi/RHIRenderPass.h>
#include <rhi/RHISampler.h>
#include <rhi/RHITexture.h>
#include <vector>

namespace VKE::Renderer {

    // Draws the offscreen color target over the whole output, turning the scene's
    // high dynamic range into displayable color.
    class TonemapPass {
    public:
        TonemapPass(RHI::RHIDevice& device, RHI::RHIRenderPass& outputPass);

        // Points the pass at one source texture per frame in flight. Call again
        // whenever those textures are recreated.
        void SetSources(const std::vector<RHI::RHITexture*>& sources);

        void Record(RHI::RHICommandBuffer& cmd, u32 frameIndex, f32 exposure);

    private:
        RHI::RHIDevice& m_Device;

        Iunique<RHI::RHIBindGroupLayout> m_Layout;
        Iunique<RHI::RHISampler> m_Sampler;
        Iunique<RHI::RHIPipeline> m_Pipeline;
        std::vector<Iunique<RHI::RHIBindGroup>> m_BindGroups;
    };

} // namespace VKE::Renderer
