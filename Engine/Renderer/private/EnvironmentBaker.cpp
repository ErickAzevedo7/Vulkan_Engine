#include "EnvironmentBaker.h"

#include "CubeFaces.h"
#include "ShaderLayouts.h"

#include <core/Log.h>
#include <math/MathTypes.h>
#include <platform/FileSystem.h>
#include <rhi/RHICommandBuffer.h>
#include <rhi/RHIFramebuffer.h>

#include <algorithm>
#include <array>
#include <vector>

namespace VKE {

namespace {
constexpr u32 kRadianceSize = 512;
constexpr u32 kIrradianceSize = 32;
constexpr u32 kPrefilterSize = 128;
constexpr u32 kPrefilterMips = 5; // Matches kMaxReflectionLod in the shaders.
constexpr u32 kBrdfLutSize = 512;

constexpr Format kCubemapFormat = Format::R16G16B16A16_SFLOAT;
constexpr Format kBrdfLutFormat = Format::R16G16_SFLOAT;
constexpr u32 kCubeVertexCount = 36;
constexpr u32 kFullscreenVertexCount = 3;
} // namespace

EnvironmentBaker::EnvironmentBaker(RHIDevice &device) : m_Device(device) {
	RenderPassDesc cubePassDesc;
	AttachmentDesc cubeFace;
	cubeFace.Format = kCubemapFormat;
	cubeFace.SampledAfter = true;
	cubePassDesc.ColorAttachments = { cubeFace };
	m_FacePass = RHIRenderPass::Create(m_Device, cubePassDesc);

	RenderPassDesc brdfPassDesc;
	AttachmentDesc brdfTarget;
	brdfTarget.Format = kBrdfLutFormat;
	brdfTarget.SampledAfter = true;
	brdfPassDesc.ColorAttachments = { brdfTarget };
	m_BrdfPass = RHIRenderPass::Create(m_Device, brdfPassDesc);

	BindGroupLayoutDesc layoutDesc;
	layoutDesc.Slots = { { 0, BindingType::CombinedImageSampler, ShaderStageFlags::Fragment } };
	layoutDesc.DebugName = "EnvironmentSourceLayout";
	m_SourceLayout = RHIBindGroupLayout::Create(m_Device, layoutDesc);

	SamplerDesc samplerDesc;
	samplerDesc.AddressModeU = SamplerAddressMode::ClampToEdge;
	samplerDesc.AddressModeV = SamplerAddressMode::ClampToEdge;
	samplerDesc.AddressModeW = SamplerAddressMode::ClampToEdge;
	samplerDesc.MaxAnisotropy = 1.0f;
	samplerDesc.DebugName = "EnvironmentSourceSampler";
	m_Sampler = RHISampler::Create(m_Device, samplerDesc);

	const auto shaderDir = FileSystem::GetExecutableDirectory() / "shaders";

	// The cube is drawn from inside, so its faces point away from the camera.
	PipelineDesc cubeDesc;
	cubeDesc.VertexShaderPath = (shaderDir / "cubemap.vert.spv").string();
	cubeDesc.PushConstantRanges = { { ShaderStageFlags::AllGraphics, 0, sizeof(CapturePushConstants) } };
	cubeDesc.CullMode = CullMode::None;

	cubeDesc.FragmentShaderPath = (shaderDir / "equirect_to_cubemap.frag.spv").string();
	cubeDesc.BindGroupLayouts = { m_SourceLayout.get() };
	m_EquirectPipeline = RHIPipeline::Create(m_Device, *m_FacePass, cubeDesc);

	cubeDesc.FragmentShaderPath = (shaderDir / "irradiance.frag.spv").string();
	m_IrradiancePipeline = RHIPipeline::Create(m_Device, *m_FacePass, cubeDesc);

	cubeDesc.FragmentShaderPath = (shaderDir / "prefilter.frag.spv").string();
	m_PrefilterPipeline = RHIPipeline::Create(m_Device, *m_FacePass, cubeDesc);

	cubeDesc.FragmentShaderPath = (shaderDir / "procedural_sky.frag.spv").string();
	cubeDesc.BindGroupLayouts.clear();
	m_SkyPipeline = RHIPipeline::Create(m_Device, *m_FacePass, cubeDesc);

	PipelineDesc brdfDesc;
	brdfDesc.VertexShaderPath = (shaderDir / "fullscreen.vert.spv").string();
	brdfDesc.FragmentShaderPath = (shaderDir / "brdf_lut.frag.spv").string();
	brdfDesc.CullMode = CullMode::None;
	m_BrdfPipeline = RHIPipeline::Create(m_Device, *m_BrdfPass, brdfDesc);
}

Iunique<GpuEnvironment> EnvironmentBaker::Bake(RHITexture *source) {
	auto environment = MakeIunique<GpuEnvironment>();

	environment->Radiance = CreateCubemap(kRadianceSize, 0, "EnvironmentRadiance");
	Iunique<RHIBindGroup> sourceGroup = source ? CreateSourceGroup(*source) : nullptr;
	RHIPipeline &radiancePipeline = source ? *m_EquirectPipeline : *m_SkyPipeline;

	// Every mip is rendered from the source, so the prefilter pass can read
	// whichever one matches the part of the sphere a sample covers.
	for (u32 mip = 0; mip < environment->Radiance->GetMipLevels(); ++mip) {
		RenderCubeFaces(*environment->Radiance, radiancePipeline, sourceGroup.get(), mip, 0.0f);
	}

	Iunique<RHIBindGroup> radianceGroup = CreateSourceGroup(*environment->Radiance);

	environment->Irradiance = CreateCubemap(kIrradianceSize, 1, "EnvironmentIrradiance");
	RenderCubeFaces(*environment->Irradiance, *m_IrradiancePipeline, radianceGroup.get(), 0, 0.0f);

	environment->Prefilter = CreateCubemap(kPrefilterSize, kPrefilterMips, "EnvironmentPrefilter");
	for (u32 mip = 0; mip < kPrefilterMips; ++mip) {
		f32 roughness = static_cast<f32>(mip) / static_cast<f32>(kPrefilterMips - 1);
		RenderCubeFaces(*environment->Prefilter, *m_PrefilterPipeline, radianceGroup.get(), mip, roughness);
	}

	environment->BrdfLut = BakeBrdfLut();

	VKE_CORE_INFO("Renderer: baked the environment from {}", source ? "an image" : "the stand-in sky");
	return environment;
}

Iunique<RHITexture> EnvironmentBaker::CreateCubemap(u32 size, u32 mipLevels, const char *debugName) {
	TextureDesc desc;
	desc.Width = size;
	desc.Height = size;
	desc.ArrayLayers = 6;
	desc.MipLevels = mipLevels;
	desc.Type = TextureType::TextureCube;
	desc.Format = kCubemapFormat;
	desc.Usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
	desc.DebugName = debugName;
	return RHITexture::Create(m_Device, desc);
}

Iunique<RHIBindGroup> EnvironmentBaker::CreateSourceGroup(RHITexture &texture) {
	BindGroupDesc groupDesc;
	groupDesc.Layout = m_SourceLayout.get();
	groupDesc.DebugName = "EnvironmentSource";

	BoundResource resource;
	resource.Binding = 0;
	resource.Texture = &texture;
	resource.Sampler = m_Sampler.get();
	groupDesc.Resources = { resource };

	return RHIBindGroup::Create(m_Device, groupDesc);
}

void EnvironmentBaker::RenderCubeFaces(
		RHITexture &target, RHIPipeline &pipeline, RHIBindGroup *sourceGroup, u32 mipLevel, f32 roughness) {
	u32 size = std::max(target.GetWidth() >> mipLevel, 1u);

	std::vector<Iunique<RHIFramebuffer>> faceTargets;
	faceTargets.reserve(6);
	for (u32 face = 0; face < 6; ++face) {
		FramebufferDesc framebufferDesc;
		framebufferDesc.AttachmentViews = { target.GetAttachmentView(mipLevel, face) };
		framebufferDesc.Width = size;
		framebufferDesc.Height = size;
		faceTargets.push_back(RHIFramebuffer::Create(m_Device, *m_FacePass, framebufferDesc));
	}

	const Mat4 projection = GetCubeFaceProjection(0.1f, 10.0f);
	const std::array<Mat4, 6> views = GetCubeFaceViews(Vec3(0.0f));

	m_Device.ExecuteImmediate([&](RHICommandBuffer &cmd) {
		for (u32 face = 0; face < 6; ++face) {
			ClearValue clear;
			cmd.BeginRenderPass(*m_FacePass, *faceTargets[face], clear);
			cmd.BindPipeline(pipeline);
			if (sourceGroup) {
				cmd.BindBindGroup(0, *sourceGroup);
			}

			CapturePushConstants constants{};
			constants.ViewProjection = projection * views[face];
			constants.Parameters = Vec4(roughness, 0.0f, 0.0f, 0.0f);
			cmd.PushConstants(ShaderStageFlags::AllGraphics, 0, sizeof(constants), &constants);

			cmd.Draw(kCubeVertexCount, 1, 0, 0);
			cmd.EndRenderPass();
		}
	});
}

Iunique<RHITexture> EnvironmentBaker::BakeBrdfLut() {
	TextureDesc desc;
	desc.Width = kBrdfLutSize;
	desc.Height = kBrdfLutSize;
	desc.Format = kBrdfLutFormat;
	desc.Usage = TextureUsage::ColorAttachment | TextureUsage::Sampled;
	desc.DebugName = "EnvironmentBrdfLut";
	Iunique<RHITexture> brdfLut = RHITexture::Create(m_Device, desc);

	FramebufferDesc framebufferDesc;
	framebufferDesc.AttachmentViews = { brdfLut->GetAttachmentView(0, 0) };
	framebufferDesc.Width = kBrdfLutSize;
	framebufferDesc.Height = kBrdfLutSize;
	Iunique<RHIFramebuffer> target = RHIFramebuffer::Create(m_Device, *m_BrdfPass, framebufferDesc);

	m_Device.ExecuteImmediate([&](RHICommandBuffer &cmd) {
		ClearValue clear;
		cmd.BeginRenderPass(*m_BrdfPass, *target, clear);
		cmd.BindPipeline(*m_BrdfPipeline);
		cmd.Draw(kFullscreenVertexCount, 1, 0, 0);
		cmd.EndRenderPass();
	});

	return brdfLut;
}

} // namespace VKE
