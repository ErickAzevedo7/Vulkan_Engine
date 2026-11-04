#pragma once

#include <core/Types.h>
#include <rhi/RHIBindGroup.h>
#include <rhi/RHICommandBuffer.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHIPipeline.h>
#include <rhi/RHIRenderPass.h>
#include <rhi/RHISampler.h>
#include <rhi/RHITexture.h>

#include <unordered_map>

namespace VKE {

// Draws a high dynamic range texture over the whole output of a render pass, turning it
// into displayable color.
class TonemapPass {
public:
	explicit TonemapPass(RHIDevice &device);

	// The bind group Record() reads `source` through. Make a new one whenever the
	// texture is recreated.
	Iunique<RHIBindGroup> CreateSource(RHITexture &source);

	// Must be called inside `outputPass`. `encodeSrgb` writes sRGB-encoded values, for
	// outputs whose format does not encode by itself (UNORM); otherwise the result stays linear.
	void Record(RHICommandBuffer &cmd, RHIRenderPass &outputPass, RHIBindGroup &source, f32 exposure, bool encodeSrgb);

private:
	RHIPipeline &GetPipeline(RHIRenderPass &outputPass);

	RHIDevice &m_Device;

	Iunique<RHIBindGroupLayout> m_Layout;
	Iunique<RHISampler> m_Sampler;
	// One per output pass it has drawn in; the passes must outlive this.
	std::unordered_map<const RHIRenderPass *, Iunique<RHIPipeline>> m_Pipelines;
};

} // namespace VKE
