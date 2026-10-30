#pragma once

#include "DrawItem.h"
#include "ShaderLayouts.h"
#include "ShadowAtlas.h"

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

    class LightClusters;

    // Renders the depth of the scene as seen from each shadowed light into one atlas, which the
    // forward shader then samples. Directional and spot lights take one tile of it and point
    // lights six. Each light's resolution goes by how much of the screen it can reach, so the
    // number of shadows is only limited by the atlas's room: when it runs out, the least
    // important lights lose resolution first.
    class ShadowPass {
    public:
        explicit ShadowPass(RHI::RHIDevice& device);

        RHI::RHITexture& GetAtlas() { return *m_Atlas; }
        RHI::RHISampler& GetSampler() { return *m_Sampler; }

        // Divides the atlas among the frame's lights that cast shadows, writing their shadow
        // data into `outShadows` and their indices into the lights. `pixelsAtOneMeter` is how
        // tall on screen something one meter wide looks from one meter away.
        // `sceneBounds` is what a directional light's shadow has to cover.
        void Prepare(LightClusters& lights, const Vec3& viewPosition, f32 pixelsAtOneMeter, const AABB& sceneBounds,
                     std::vector<GpuShadow>& outShadows);
        // No shadows this frame.
        void Clear() { m_Views.clear(); }

        // Must be called outside any render pass.
        void Record(RHI::RHICommandBuffer& cmd, const std::vector<DrawItem>& items);

    private:
        // One depth render of the scene, into one tile of the atlas.
        struct ShadowView {
            Mat4 ViewProjection;
            ShadowTile Tile;
            // Lights with a range only see the items that touch this sphere.
            bool CullToRange;
            Vec3 Position;
            f32 Range;
        };

        Iunique<RHI::RHIRenderPass> m_RenderPass;
        Iunique<RHI::RHITexture> m_Atlas;
        Iunique<RHI::RHIFramebuffer> m_Framebuffer;
        Iunique<RHI::RHIPipeline> m_Pipeline;
        Iunique<RHI::RHISampler> m_Sampler;

        u32 m_AtlasSize = 0;
        std::vector<ShadowView> m_Views;
        std::vector<ShadowRequest> m_Requests;
        std::vector<ShadowTile> m_Tiles;
        bool m_SpaceWarned = false;
    };

} // namespace VKE::Renderer
