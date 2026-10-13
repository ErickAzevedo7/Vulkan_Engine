#pragma once

#include <core/Types.h>
#include <rhi/RHIBindGroup.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHIPipeline.h>
#include <rhi/RHIRenderPass.h>
#include <rhi/RHISampler.h>
#include <rhi/RHITexture.h>

namespace VKE::Renderer {

    // The maps baked from one environment: what it looks like, plus the two
    // convolutions and the lookup table image based lighting needs.
    struct GpuEnvironment {
        Iunique<RHI::RHITexture> Radiance;
        Iunique<RHI::RHITexture> Irradiance;
        Iunique<RHI::RHITexture> Prefilter;
        Iunique<RHI::RHITexture> BrdfLut;
    };

    // Renders an equirectangular image, or a stand-in sky, into those maps.
    // Runs outside the frame loop, once per environment.
    class EnvironmentBaker {
    public:
        explicit EnvironmentBaker(RHI::RHIDevice& device);

        // `source` is the equirectangular image; nullptr bakes the stand-in sky.
        Iunique<GpuEnvironment> Bake(RHI::RHITexture* source);

    private:
        Iunique<RHI::RHITexture> CreateCubemap(u32 size, u32 mipLevels, const char* debugName);
        Iunique<RHI::RHIBindGroup> CreateSourceGroup(RHI::RHITexture& texture);
        void RenderCubeFaces(RHI::RHITexture& target, RHI::RHIPipeline& pipeline, RHI::RHIBindGroup* sourceGroup,
                             u32 mipLevel, f32 roughness);
        Iunique<RHI::RHITexture> BakeBrdfLut();

        RHI::RHIDevice& m_Device;

        Iunique<RHI::RHIRenderPass> m_FacePass;
        Iunique<RHI::RHIRenderPass> m_BrdfPass;
        Iunique<RHI::RHIBindGroupLayout> m_SourceLayout;
        Iunique<RHI::RHISampler> m_Sampler;

        Iunique<RHI::RHIPipeline> m_EquirectPipeline;
        Iunique<RHI::RHIPipeline> m_SkyPipeline;
        Iunique<RHI::RHIPipeline> m_IrradiancePipeline;
        Iunique<RHI::RHIPipeline> m_PrefilterPipeline;
        Iunique<RHI::RHIPipeline> m_BrdfPipeline;
    };

} // namespace VKE::Renderer
