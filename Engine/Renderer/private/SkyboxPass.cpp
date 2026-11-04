#include "SkyboxPass.h"

#include "ShaderLayouts.h"

#include <platform/FileSystem.h>

namespace VKE {

namespace {
constexpr u32 kCubeVertexCount = 36;
} // namespace

SkyboxPass::SkyboxPass(RHIDevice &device, RHIRenderPass &scenePass, RHIBindGroupLayout &frameLayout,
		RHIBindGroupLayout &environmentLayout) {
	const auto shaderDir = FileSystem::GetExecutableDirectory() / "shaders";

	PipelineDesc desc;
	desc.VertexShaderPath = (shaderDir / "skybox.vert.spv").string();
	desc.FragmentShaderPath = (shaderDir / "skybox.frag.spv").string();
	desc.BindGroupLayouts = { &frameLayout, &environmentLayout };
	desc.CullMode = CullMode::None;
	// Drawn after the scene, at the far plane, so it only fills empty pixels.
	desc.DepthTestEnable = true;
	desc.DepthWriteEnable = false;
	desc.DepthCompareOp = CompareOp::LessOrEqual;
	m_Pipeline = RHIPipeline::Create(device, scenePass, desc);
}

void SkyboxPass::Record(RHICommandBuffer &cmd, RHIBindGroup &frameGroup, RHIBindGroup &environmentGroup) {
	cmd.BindPipeline(*m_Pipeline);
	cmd.BindBindGroup(kFrameSet, frameGroup);
	cmd.BindBindGroup(kEnvironmentSet, environmentGroup);
	cmd.Draw(kCubeVertexCount, 1, 0, 0);
}

} // namespace VKE
