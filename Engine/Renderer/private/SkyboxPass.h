#pragma once

#include <core/Types.h>
#include <rhi/RHIBindGroup.h>
#include <rhi/RHICommandBuffer.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHIPipeline.h>
#include <rhi/RHIRenderPass.h>

namespace VKE {

// Fills whatever the scene did not cover with the environment cubemap.
class SkyboxPass {
public:
	SkyboxPass(RHIDevice &device, RHIRenderPass &scenePass, RHIBindGroupLayout &frameLayout,
			RHIBindGroupLayout &environmentLayout);

	void Record(RHICommandBuffer &cmd, RHIBindGroup &frameGroup, RHIBindGroup &environmentGroup);

private:
	Iunique<RHIPipeline> m_Pipeline;
};

} // namespace VKE
