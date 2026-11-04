#include "TonemapPass.h"

#include <platform/FileSystem.h>

namespace VKE {

namespace {
// Must match TonemapConstants in shaders/tonemap.frag.
struct TonemapConstants {
	f32 Exposure;
	u32 EncodeSrgb;
};
} // namespace

TonemapPass::TonemapPass(RHIDevice &device) : m_Device(device) {
	BindGroupLayoutDesc layoutDesc;
	layoutDesc.Slots = { { 0, BindingType::CombinedImageSampler, ShaderStageFlags::Fragment } };
	layoutDesc.DebugName = "TonemapLayout";
	m_Layout = RHIBindGroupLayout::Create(m_Device, layoutDesc);

	SamplerDesc samplerDesc;
	samplerDesc.AddressModeU = SamplerAddressMode::ClampToEdge;
	samplerDesc.AddressModeV = SamplerAddressMode::ClampToEdge;
	samplerDesc.MaxAnisotropy = 1.0f;
	samplerDesc.DebugName = "TonemapSampler";
	m_Sampler = RHISampler::Create(m_Device, samplerDesc);
}

RHIPipeline &TonemapPass::GetPipeline(RHIRenderPass &outputPass) {
	Iunique<RHIPipeline> &pipeline = m_Pipelines[&outputPass];
	if (pipeline) {
		return *pipeline;
	}

	const auto shaderDir = FileSystem::GetExecutableDirectory() / "shaders";

	// The vertex shader builds a full-screen triangle, so there is no vertex input.
	PipelineDesc pipelineDesc;
	pipelineDesc.VertexShaderPath = (shaderDir / "fullscreen.vert.spv").string();
	pipelineDesc.FragmentShaderPath = (shaderDir / "tonemap.frag.spv").string();
	pipelineDesc.BindGroupLayouts = { m_Layout.get() };
	pipelineDesc.PushConstantRanges = { { ShaderStageFlags::Fragment, 0, sizeof(TonemapConstants) } };
	pipelineDesc.CullMode = CullMode::None;
	pipeline = RHIPipeline::Create(m_Device, outputPass, pipelineDesc);
	return *pipeline;
}

Iunique<RHIBindGroup> TonemapPass::CreateSource(RHITexture &source) {
	BindGroupDesc groupDesc;
	groupDesc.Layout = m_Layout.get();
	groupDesc.DebugName = "TonemapSource";

	BoundResource resource;
	resource.Binding = 0;
	resource.Texture = &source;
	resource.Sampler = m_Sampler.get();
	groupDesc.Resources = { resource };

	return RHIBindGroup::Create(m_Device, groupDesc);
}

void TonemapPass::Record(
		RHICommandBuffer &cmd, RHIRenderPass &outputPass, RHIBindGroup &source, f32 exposure, bool encodeSrgb) {
	TonemapConstants constants{ exposure, encodeSrgb ? 1u : 0u };
	cmd.BindPipeline(GetPipeline(outputPass));
	cmd.BindBindGroup(0, source);
	cmd.PushConstants(ShaderStageFlags::Fragment, 0, sizeof(constants), &constants);
	cmd.Draw(3, 1, 0, 0);
}

} // namespace VKE
