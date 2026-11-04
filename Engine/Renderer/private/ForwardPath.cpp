#include "ForwardPath.h"

#include "ShaderLayouts.h"

#include <assets/AssetManager.h>
#include <core/Log.h>
#include <platform/FileSystem.h>
#include <renderer/RenderScene.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <functional>

namespace VKE {

namespace {
static_assert(sizeof(Vertex) == 48, "Vertex layout changed, update the pipeline attributes");

// Keeps light values above 1.0 instead of clipping them at the swapchain.
constexpr Format kSceneColorFormat = Format::R16G16B16A16_SFLOAT;

// Box around the eight transformed corners of `bounds`.
AABB TransformBounds(const AABB &bounds, const Mat4 &transform) {
	AABB result(Vec3(FLT_MAX), Vec3(-FLT_MAX));
	for (u32 corner = 0; corner < 8; ++corner) {
		Vec3 point((corner & 1) ? bounds.Max.x : bounds.Min.x, (corner & 2) ? bounds.Max.y : bounds.Min.y,
				(corner & 4) ? bounds.Max.z : bounds.Min.z);
		Vec3 world = Vec3(transform * Vec4(point, 1.0f));
		result.Min = glm::min(result.Min, world);
		result.Max = glm::max(result.Max, world);
	}
	return result;
}

// Replaces `buffer` with a larger one if it can't hold `size` bytes. Returns whether it did.
bool Reserve(RHIDevice &device, Iunique<RHIBuffer> &buffer, size_t size, const char *debugName) {
	if (buffer && buffer->GetSize() >= size) {
		return false;
	}

	// Room to grow, so a scene that adds lights one by one doesn't replace it every frame.
	BufferDesc desc;
	desc.Size = std::max<size_t>(size + size / 2, 4096);
	desc.Usage = BufferUsage::Storage;
	desc.Memory = BufferMemory::HostVisible;
	desc.DebugName = debugName;
	buffer = RHIBuffer::Create(device, desc);
	return true;
}

template <typename T> void WriteList(RHIBuffer &buffer, const std::vector<T> &list) {
	if (!list.empty()) {
		buffer.Write(list.data(), list.size() * sizeof(T));
	}
}
} // namespace

ForwardPath::ForwardPath(RHIDevice &device, AssetManager &assets, u32 framesInFlight) :
		m_Device(device), m_FramesInFlight(framesInFlight) {
	RenderPassDesc scenePassDesc;
	AttachmentDesc sceneColor;
	sceneColor.Format = kSceneColorFormat;
	sceneColor.SampledAfter = true;
	scenePassDesc.ColorAttachments = { sceneColor };
	scenePassDesc.HasDepthAttachment = true;
	scenePassDesc.DepthAttachment.Format = m_Device.GetDepthFormat();
	scenePassDesc.DepthAttachment.IsDepth = true;
	m_ScenePass = RHIRenderPass::Create(m_Device, scenePassDesc);

	BindGroupLayoutDesc frameLayoutDesc;
	frameLayoutDesc.Slots = { { 0, BindingType::UniformBuffer, ShaderStageFlags::AllGraphics } };
	for (u32 binding : { kLightListBinding, kClusterListBinding, kLightIndexListBinding, kShadowListBinding }) {
		frameLayoutDesc.Slots.push_back({ binding, BindingType::StorageBuffer, ShaderStageFlags::Fragment });
	}
	frameLayoutDesc.DebugName = "FrameLayout";
	m_FrameLayout = RHIBindGroupLayout::Create(m_Device, frameLayoutDesc);

	BindGroupLayoutDesc environmentLayoutDesc;
	for (u32 binding :
			{ kIrradianceBinding, kPrefilterBinding, kBrdfLutBinding, kRadianceBinding, kShadowAtlasBinding }) {
		environmentLayoutDesc.Slots.push_back(
				{ binding, BindingType::CombinedImageSampler, ShaderStageFlags::Fragment });
	}
	environmentLayoutDesc.DebugName = "EnvironmentLayout";
	m_EnvironmentLayout = RHIBindGroupLayout::Create(m_Device, environmentLayoutDesc);

	BindGroupLayoutDesc materialLayoutDesc;
	for (u32 slot = 0; slot < static_cast<u32>(MaterialTexture::Count); ++slot) {
		materialLayoutDesc.Slots.push_back({ slot, BindingType::CombinedImageSampler, ShaderStageFlags::Fragment });
	}
	materialLayoutDesc.Slots.push_back(
			{ kMaterialUniformsBinding, BindingType::UniformBuffer, ShaderStageFlags::Fragment });
	materialLayoutDesc.DebugName = "MaterialLayout";
	m_MaterialLayout = RHIBindGroupLayout::Create(m_Device, materialLayoutDesc);

	CreatePipeline();

	m_GpuCache = MakeIunique<GpuCache>(m_Device, assets, *m_MaterialLayout, framesInFlight);

	SamplerDesc environmentSamplerDesc;
	environmentSamplerDesc.AddressModeU = SamplerAddressMode::ClampToEdge;
	environmentSamplerDesc.AddressModeV = SamplerAddressMode::ClampToEdge;
	environmentSamplerDesc.AddressModeW = SamplerAddressMode::ClampToEdge;
	environmentSamplerDesc.MaxAnisotropy = 1.0f;
	environmentSamplerDesc.DebugName = "EnvironmentSampler";
	m_EnvironmentSampler = RHISampler::Create(m_Device, environmentSamplerDesc);

	m_ShadowPass = MakeIunique<ShadowPass>(m_Device);
	m_SkyboxPass = MakeIunique<SkyboxPass>(m_Device, *m_ScenePass, *m_FrameLayout, *m_EnvironmentLayout);
	m_TonemapPass = MakeIunique<TonemapPass>(m_Device);
}

ForwardPath::~ForwardPath() = default;

void ForwardPath::CreatePipeline() {
	const auto shaderDir = FileSystem::GetExecutableDirectory() / "shaders";

	PipelineDesc desc;
	desc.VertexShaderPath = (shaderDir / "forward.vert.spv").string();
	desc.FragmentShaderPath = (shaderDir / "forward.frag.spv").string();
	desc.VertexStride = sizeof(Vertex);
	desc.VertexAttributes = {
		{ 0, Format::R32G32B32_SFLOAT, offsetof(Vertex, Position) },
		{ 1, Format::R32G32B32_SFLOAT, offsetof(Vertex, Normal) },
		{ 2, Format::R32G32_SFLOAT, offsetof(Vertex, UV) },
		{ 3, Format::R32G32B32A32_SFLOAT, offsetof(Vertex, Tangent) },
	};
	desc.BindGroupLayouts = { m_FrameLayout.get(), m_EnvironmentLayout.get(), m_MaterialLayout.get() };
	desc.PushConstantRanges = { { ShaderStageFlags::Vertex, 0, sizeof(ObjectPushConstants) } };
	desc.CullMode = CullMode::Back;
	desc.FrontFace = FrontFace::CounterClockwise;
	desc.DepthTestEnable = true;
	desc.DepthWriteEnable = true;
	m_Pipeline = RHIPipeline::Create(m_Device, *m_ScenePass, desc);
}

Iunique<SceneTargets> ForwardPath::CreateTargets(u32 width, u32 height, const char *debugName) {
	auto targets = MakeIunique<SceneTargets>();
	targets->Width = width;
	targets->Height = height;

	// One of each per frame in flight, so a frame never draws over what the previous
	// one is still reading.
	targets->Targets.resize(m_FramesInFlight);
	for (u32 i = 0; i < m_FramesInFlight; ++i) {
		targets->Targets[i].Create(
				m_Device, *m_ScenePass, width, height, kSceneColorFormat, m_Device.GetDepthFormat(), debugName);

		BufferDesc uniformDesc;
		uniformDesc.Size = sizeof(FrameUniforms);
		uniformDesc.Usage = BufferUsage::Uniform;
		uniformDesc.Memory = BufferMemory::HostVisible;
		uniformDesc.DebugName = "FrameUniforms";
		FrameData frame;
		frame.Uniforms = RHIBuffer::Create(m_Device, uniformDesc);
		targets->Frames.push_back(std::move(frame));

		targets->TonemapSources.push_back(m_TonemapPass->CreateSource(*targets->Targets[i].Color));
	}
	return targets;
}

void ForwardPath::CollectDrawItems(const RenderScene &scene) {
	m_DrawItems.clear();
	m_SceneBounds = AABB(Vec3(FLT_MAX), Vec3(-FLT_MAX));

	for (const MeshInstanceDesc &instance : scene.GetMeshes().Values()) {
		if (!instance.Visible) {
			continue;
		}

		const GpuMesh *mesh = m_GpuCache->GetMesh(instance.Mesh);
		if (!mesh) {
			continue;
		}

		for (size_t i = 0; i < mesh->Submeshes.size(); ++i) {
			const Submesh &submesh = mesh->Submeshes[i];
			Handle<MaterialData> material;
			if (submesh.MaterialIndex < instance.Materials.size()) {
				material = instance.Materials[submesh.MaterialIndex];
			} else if (submesh.MaterialIndex < mesh->Materials.size()) {
				material = mesh->Materials[submesh.MaterialIndex];
			}

			AABB worldBounds = TransformBounds(mesh->SubmeshBounds[i], instance.Transform);
			m_SceneBounds.Merge(worldBounds);
			m_DrawItems.push_back({ &m_GpuCache->GetMaterial(material), mesh, &submesh, &instance, worldBounds });
		}
	}

	// Grouping by material, then mesh, keeps bind group and buffer changes to a minimum.
	std::sort(m_DrawItems.begin(), m_DrawItems.end(), [](const DrawItem &a, const DrawItem &b) {
		if (a.Material != b.Material) {
			return std::less<const GpuMaterial *>()(a.Material, b.Material);
		}
		return std::less<const GpuMesh *>()(a.Mesh, b.Mesh);
	});
}

RHIBindGroup &ForwardPath::GetEnvironmentGroup(const RenderScene &scene) {
	const EnvironmentDesc &environment = scene.GetEnvironment();
	const GpuEnvironment &maps = m_GpuCache->GetEnvironmentMaps(environment.Source);
	Iunique<RHIBindGroup> &group = m_EnvironmentGroups[&maps];
	if (group) {
		return *group;
	}

	BindGroupDesc groupDesc;
	groupDesc.Layout = m_EnvironmentLayout.get();
	groupDesc.DebugName = "EnvironmentGroup";

	auto bind = [&](u32 binding, RHITexture *texture, RHISampler *sampler) {
		BoundResource resource;
		resource.Binding = binding;
		resource.Texture = texture;
		resource.Sampler = sampler;
		groupDesc.Resources.push_back(resource);
	};
	bind(kIrradianceBinding, maps.Irradiance.get(), m_EnvironmentSampler.get());
	bind(kPrefilterBinding, maps.Prefilter.get(), m_EnvironmentSampler.get());
	bind(kBrdfLutBinding, maps.BrdfLut.get(), m_EnvironmentSampler.get());
	bind(kRadianceBinding, maps.Radiance.get(), m_EnvironmentSampler.get());
	bind(kShadowAtlasBinding, &m_ShadowPass->GetAtlas(), &m_ShadowPass->GetSampler());

	group = RHIBindGroup::Create(m_Device, groupDesc);
	return *group;
}

void ForwardPath::WriteFrameData(
		FrameData &data, const RenderScene &scene, const RenderView &view, u32 width, u32 height) {
	FrameUniforms frame{};
	frame.View = view.View;
	frame.Projection = view.GetProjection(static_cast<f32>(width) / static_cast<f32>(height));
	frame.CameraPosition = Vec4(view.Position, 1.0f);
	frame.EnvironmentParameters = Vec4(scene.GetEnvironment().Intensity, 0.0f, 0.0f, 0.0f);

	m_LightClusters.Build(scene.GetLights().Values(), view, frame.Projection, width, height, frame);

	m_Shadows.clear();
	if (m_ShadowsEnabled && !m_DrawItems.empty()) {
		f32 pixelsAtOneMeter = 0.5f * static_cast<f32>(height) * std::abs(frame.Projection[1][1]);
		m_ShadowPass->Prepare(m_LightClusters, view.Position, pixelsAtOneMeter, m_SceneBounds, m_Shadows);
	} else {
		m_ShadowPass->Clear();
	}

	const std::vector<GpuLight> &lights = m_LightClusters.GetLights();
	const std::vector<GpuCluster> &clusters = m_LightClusters.GetClusters();
	const std::vector<u32> &indices = m_LightClusters.GetIndices();

	bool replaced = Reserve(m_Device, data.Lights, lights.size() * sizeof(GpuLight), "LightList");
	replaced |= Reserve(m_Device, data.Clusters, clusters.size() * sizeof(GpuCluster), "ClusterList");
	replaced |= Reserve(m_Device, data.LightIndices, indices.size() * sizeof(u32), "LightIndexList");
	replaced |= Reserve(m_Device, data.Shadows, m_Shadows.size() * sizeof(GpuShadow), "ShadowList");
	if (replaced) {
		BindGroupDesc groupDesc;
		groupDesc.Layout = m_FrameLayout.get();
		groupDesc.Resources = { { 0, data.Uniforms.get() }, { kLightListBinding, data.Lights.get() },
			{ kClusterListBinding, data.Clusters.get() }, { kLightIndexListBinding, data.LightIndices.get() },
			{ kShadowListBinding, data.Shadows.get() } };
		groupDesc.DebugName = "FrameGroup";
		data.Group = RHIBindGroup::Create(m_Device, groupDesc);
	}

	data.Uniforms->Write(&frame, sizeof(frame));
	WriteList(*data.Lights, lights);
	WriteList(*data.Clusters, clusters);
	WriteList(*data.LightIndices, indices);
	WriteList(*data.Shadows, m_Shadows);
}

void ForwardPath::Record(RHICommandBuffer &cmd, u32 frameIndex, const RenderScene &scene, const RenderView &view,
		SceneTargets &targets) {
	// Uploads any assets seen for the first time.
	CollectDrawItems(scene);
	RHIBindGroup &environmentGroup = GetEnvironmentGroup(scene);

	FrameData &frameData = targets.Frames[frameIndex];
	WriteFrameData(frameData, scene, view, targets.Width, targets.Height);
	RHIBindGroup &frameGroup = *frameData.Group;

	m_ShadowPass->Record(cmd, m_DrawItems);

	ClearValue sceneClear;
	sceneClear.R = 0.02f;
	sceneClear.G = 0.02f;
	sceneClear.B = 0.05f;

	cmd.BeginRenderPass(*m_ScenePass, *targets.Targets[frameIndex].Framebuffer, sceneClear);
	cmd.BindPipeline(*m_Pipeline);
	cmd.BindBindGroup(kFrameSet, frameGroup);
	cmd.BindBindGroup(kEnvironmentSet, environmentGroup);

	const GpuMaterial *boundMaterial = nullptr;
	const GpuMesh *boundMesh = nullptr;
	for (const DrawItem &item : m_DrawItems) {
		if (item.Material != boundMaterial) {
			cmd.BindBindGroup(kMaterialSet, *item.Material->BindGroup);
			boundMaterial = item.Material;
		}
		if (item.Mesh != boundMesh) {
			cmd.BindVertexBuffer(*item.Mesh->VertexBuffer);
			cmd.BindIndexBuffer(*item.Mesh->IndexBuffer, IndexType::U32);
			boundMesh = item.Mesh;
		}

		ObjectPushConstants object;
		object.Model = item.Instance->Transform;
		Mat3 normalMatrix = glm::transpose(glm::inverse(Mat3(object.Model)));
		for (int column = 0; column < 3; ++column) {
			object.NormalMatrix[column] = Vec4(normalMatrix[column], 0.0f);
		}

		cmd.PushConstants(ShaderStageFlags::Vertex, 0, sizeof(object), &object);
		cmd.DrawIndexed(item.Submesh->IndexCount, 1, item.Submesh->FirstIndex, 0, 0);
	}

	// Last in the pass, so it only touches pixels the scene left empty.
	m_SkyboxPass->Record(cmd, frameGroup, environmentGroup);

	cmd.EndRenderPass();
}

void ForwardPath::Resolve(
		RHICommandBuffer &cmd, u32 frameIndex, SceneTargets &targets, RHIRenderPass &outputPass, bool encodeSrgb) {
	m_TonemapPass->Record(cmd, outputPass, *targets.TonemapSources[frameIndex], m_Exposure, encodeSrgb);
}

} // namespace VKE
