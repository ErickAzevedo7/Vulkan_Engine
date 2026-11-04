#pragma once

#include <core/Types.h>
#include <rhi/RHIBindGroup.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHIPipeline.h>
#include <rhi/RHIRenderPass.h>
#include <rhi/RHISampler.h>
#include <rhi/RHITexture.h>

namespace VKE {

// The maps baked from one environment: what it looks like, plus the two
// convolutions and the lookup table image based lighting needs.
struct GpuEnvironment {
	Iunique<RHITexture> Radiance;
	Iunique<RHITexture> Irradiance;
	Iunique<RHITexture> Prefilter;
	Iunique<RHITexture> BrdfLut;
};

// Renders an equirectangular image, or a stand-in sky, into those maps.
// Runs outside the frame loop, once per environment.
class EnvironmentBaker {
public:
	explicit EnvironmentBaker(RHIDevice &device);

	// `source` is the equirectangular image; nullptr bakes the stand-in sky.
	Iunique<GpuEnvironment> Bake(RHITexture *source);

private:
	Iunique<RHITexture> CreateCubemap(u32 size, u32 mipLevels, const char *debugName);
	Iunique<RHIBindGroup> CreateSourceGroup(RHITexture &texture);
	void RenderCubeFaces(
			RHITexture &target, RHIPipeline &pipeline, RHIBindGroup *sourceGroup, u32 mipLevel, f32 roughness);
	Iunique<RHITexture> BakeBrdfLut();

	RHIDevice &m_Device;

	Iunique<RHIRenderPass> m_FacePass;
	Iunique<RHIRenderPass> m_BrdfPass;
	Iunique<RHIBindGroupLayout> m_SourceLayout;
	Iunique<RHISampler> m_Sampler;

	Iunique<RHIPipeline> m_EquirectPipeline;
	Iunique<RHIPipeline> m_SkyPipeline;
	Iunique<RHIPipeline> m_IrradiancePipeline;
	Iunique<RHIPipeline> m_PrefilterPipeline;
	Iunique<RHIPipeline> m_BrdfPipeline;
};

} // namespace VKE
