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

    // Renders the depth of the scene as seen from each shadowed light, which the
    // forward shader then samples. Directional and spot lights get one layer of a
    // texture array; point lights get one cube of a cube array.
    class ShadowPass {
    public:
        explicit ShadowPass(RHI::RHIDevice& device);

        RHI::RHITexture& GetShadowMaps() { return *m_ShadowMaps; }
        RHI::RHITexture& GetPointShadowMaps() { return *m_PointShadowMaps; }
        RHI::RHISampler& GetSampler() { return *m_Sampler; }

        // Picks the lights that get a shadow this frame and writes their shadow data
        // into `frame`. `sceneBounds` is what a directional light's shadow has to cover.
        void Prepare(const std::vector<LightDesc>& lights, u32 lightCount, const AABB& sceneBounds,
                     FrameUniforms& frame);

        // Must be called outside any render pass.
        void Record(RHI::RHICommandBuffer& cmd, const std::vector<DrawItem>& items);

    private:
        // One depth render of the scene, into one layer.
        struct ShadowView {
            Mat4 ViewProjection;
            RHI::RHIFramebuffer* Framebuffer;
            // Lights with a range only see the items that touch this sphere.
            bool CullToRange;
            Vec3 Position;
            f32 Range;
        };

        Iunique<RHI::RHIRenderPass> m_RenderPass;
        Iunique<RHI::RHITexture> m_ShadowMaps;
        Iunique<RHI::RHITexture> m_PointShadowMaps;
        std::vector<Iunique<RHI::RHIFramebuffer>> m_Framebuffers;
        // Six per cube, in face order.
        std::vector<Iunique<RHI::RHIFramebuffer>> m_PointFramebuffers;
        Iunique<RHI::RHIPipeline> m_Pipeline;
        Iunique<RHI::RHISampler> m_Sampler;

        std::vector<ShadowView> m_Views;
        bool m_LimitWarned = false;
        bool m_PointLimitWarned = false;
    };

} // namespace VKE::Renderer
