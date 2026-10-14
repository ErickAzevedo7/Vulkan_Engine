#pragma once

#include "DrawItem.h"
#include "ShaderLayouts.h"

#include <core/Types.h>
#include <math/AABB.h>
#include <renderer/RenderTypes.h>
#include <rhi/RHICommandBuffer.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHIFramebuffer.h>
#include <rhi/RHIPipeline.h>
#include <rhi/RHIRenderPass.h>
#include <rhi/RHISampler.h>
#include <rhi/RHITexture.h>
#include <vector>

namespace VKE::Renderer {

    // Renders the depth of the scene as seen from each shadowed light into one
    // layer of a shared texture array, which the forward shader then samples.
    class ShadowPass {
    public:
        explicit ShadowPass(RHI::RHIDevice& device);

        RHI::RHITexture& GetShadowMaps() { return *m_ShadowMaps; }
        RHI::RHISampler& GetSampler() { return *m_Sampler; }

        // Picks the lights that get a shadow this frame and writes their shadow data
        // into `frame`. `sceneBounds` is what a directional light's shadow has to cover.
        void Prepare(const std::vector<LightDesc>& lights, u32 lightCount, const AABB& sceneBounds,
                     FrameUniforms& frame);

        // Must be called outside any render pass.
        void Record(RHI::RHICommandBuffer& cmd, const std::vector<DrawItem>& items);

    private:
        Iunique<RHI::RHIRenderPass> m_RenderPass;
        Iunique<RHI::RHITexture> m_ShadowMaps;
        std::vector<Iunique<RHI::RHIFramebuffer>> m_Framebuffers;
        Iunique<RHI::RHIPipeline> m_Pipeline;
        Iunique<RHI::RHISampler> m_Sampler;

        Mat4 m_ViewProjections[kMaxShadows];
        u32 m_ShadowCount = 0;
        bool m_LimitWarned = false;
    };

} // namespace VKE::Renderer
