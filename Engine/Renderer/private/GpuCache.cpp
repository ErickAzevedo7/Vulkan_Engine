#include "GpuCache.h"

#include "ShaderLayouts.h"

#include <assets/AssetManager.h>
#include <core/Log.h>
#include <rhi/RHIDevice.h>

#include <algorithm>
#include <cfloat>

namespace VKE {

static_assert(kMaterialUniformsBinding == static_cast<u32>(MaterialTexture::Count),
		"Material uniforms must follow the texture bindings");

GpuCache::GpuCache(RHIDevice &device, AssetManager &assets, RHIBindGroupLayout &materialLayout, u32 framesInFlight) :
		m_Device(device), m_FramesInFlight(framesInFlight), m_Assets(assets), m_MaterialLayout(materialLayout) {
	SamplerDesc samplerDesc;
	samplerDesc.DebugName = "MaterialSampler";
	m_Sampler = RHISampler::Create(m_Device, samplerDesc);

	m_EnvironmentBaker = MakeIunique<EnvironmentBaker>(m_Device);

	MaterialData defaultMaterial;
	defaultMaterial.Name = "$default";
	defaultMaterial.Metallic = 0.0f;
	m_DefaultMaterial = m_Assets.AddMaterial(defaultMaterial);
}

GpuCache::~GpuCache() = default;

void GpuCache::BeginFrame(u64 frameNumber) {
	m_FrameNumber = frameNumber;
	auto done = [&](const RetiredMaterial &retired) { return retired.Frame + m_FramesInFlight <= frameNumber; };
	m_RetiredMaterials.erase(
			std::remove_if(m_RetiredMaterials.begin(), m_RetiredMaterials.end(), done), m_RetiredMaterials.end());
}

const GpuMesh *GpuCache::GetMesh(Handle<MeshData> handle) {
	if (auto it = m_Meshes.find(handle); it != m_Meshes.end()) {
		return it->second.get();
	}

	const MeshData *mesh = m_Assets.GetMesh(handle);
	if (!mesh || mesh->Vertices.empty() || mesh->Indices.empty()) {
		return nullptr;
	}

	auto gpuMesh = MakeIunique<GpuMesh>();

	BufferDesc vertexDesc;
	vertexDesc.Size = mesh->Vertices.size() * sizeof(Vertex);
	vertexDesc.Usage = BufferUsage::Vertex;
	vertexDesc.DebugName = mesh->Name.c_str();
	gpuMesh->VertexBuffer = RHIBuffer::Create(m_Device, vertexDesc, mesh->Vertices.data());

	BufferDesc indexDesc;
	indexDesc.Size = mesh->Indices.size() * sizeof(u32);
	indexDesc.Usage = BufferUsage::Index;
	indexDesc.DebugName = mesh->Name.c_str();
	gpuMesh->IndexBuffer = RHIBuffer::Create(m_Device, indexDesc, mesh->Indices.data());

	gpuMesh->Submeshes = mesh->Submeshes;
	gpuMesh->Materials = mesh->Materials;
	for (const Submesh &submesh : mesh->Submeshes) {
		AABB bounds(Vec3(FLT_MAX), Vec3(-FLT_MAX));
		for (u32 i = submesh.FirstIndex; i < submesh.FirstIndex + submesh.IndexCount; ++i) {
			const Vec3 &position = mesh->Vertices[mesh->Indices[i]].Position;
			bounds.Min = glm::min(bounds.Min, position);
			bounds.Max = glm::max(bounds.Max, position);
		}
		gpuMesh->SubmeshBounds.push_back(bounds);
	}

	auto [it, inserted] = m_Meshes.emplace(handle, std::move(gpuMesh));
	return it->second.get();
}

const GpuMaterial &GpuCache::GetMaterial(Handle<MaterialData> handle) {
	if (!m_Assets.GetMaterial(handle)) {
		handle = m_DefaultMaterial;
	}
	const MaterialData &material = *m_Assets.GetMaterial(handle);
	auto it = m_Materials.find(handle);
	if (it != m_Materials.end() && it->second->Revision == material.Revision) {
		return *it->second;
	}

	// Frames still in flight may use the old copy; the current one counts too.
	Iunique<GpuMaterial> gpuMaterial = CreateMaterial(material);
	if (it != m_Materials.end()) {
		m_RetiredMaterials.push_back({ m_FrameNumber, std::move(it->second) });
		it->second = std::move(gpuMaterial);
		return *it->second;
	}
	auto [added, inserted] = m_Materials.emplace(handle, std::move(gpuMaterial));
	return *added->second;
}

Iunique<GpuMaterial> GpuCache::CreateMaterial(const MaterialData &material) {
	auto gpuMaterial = MakeIunique<GpuMaterial>();
	gpuMaterial->Revision = material.Revision;

	MaterialUniforms uniforms;
	uniforms.BaseColor = material.BaseColor;
	uniforms.EmissiveAndOcclusion = Vec4(material.Emissive, material.Occlusion);
	uniforms.MetallicRoughness = Vec4(material.Metallic, material.Roughness, 0.0f, 0.0f);

	BufferDesc uniformDesc;
	uniformDesc.Size = sizeof(MaterialUniforms);
	uniformDesc.Usage = BufferUsage::Uniform;
	uniformDesc.DebugName = material.Name.c_str();
	gpuMaterial->UniformBuffer = RHIBuffer::Create(m_Device, uniformDesc, &uniforms);

	BindGroupDesc groupDesc;
	groupDesc.Layout = &m_MaterialLayout;
	groupDesc.DebugName = material.Name.c_str();
	for (u32 slot = 0; slot < static_cast<u32>(MaterialTexture::Count); ++slot) {
		BoundResource resource;
		resource.Binding = slot;
		resource.Texture = &GetTexture(material.Textures[slot]);
		resource.Sampler = m_Sampler.get();
		groupDesc.Resources.push_back(resource);
	}

	BoundResource uniformResource;
	uniformResource.Binding = kMaterialUniformsBinding;
	uniformResource.Buffer = gpuMaterial->UniformBuffer.get();
	groupDesc.Resources.push_back(uniformResource);

	gpuMaterial->BindGroup = RHIBindGroup::Create(m_Device, groupDesc);
	return gpuMaterial;
}

const GpuEnvironment &GpuCache::GetEnvironmentMaps(Handle<TextureData> source) {
	if (source.IsValid() && !m_Assets.GetTexture(source)) {
		source = {};
	}
	if (auto it = m_Environments.find(source); it != m_Environments.end()) {
		return *it->second;
	}

	RHITexture *sourceTexture = source.IsValid() ? &GetTexture(source) : nullptr;
	auto [it, inserted] = m_Environments.emplace(source, m_EnvironmentBaker->Bake(sourceTexture));
	return *it->second;
}

RHITexture &GpuCache::GetTexture(Handle<TextureData> handle) {
	if (!m_Assets.GetTexture(handle)) {
		handle = m_Assets.GetDefaultTexture(DefaultTexture::White);
	}
	if (auto it = m_Textures.find(handle); it != m_Textures.end()) {
		return *it->second;
	}

	const TextureData &texture = *m_Assets.GetTexture(handle);

	TextureDesc desc;
	desc.Width = texture.Width;
	desc.Height = texture.Height;
	if (texture.PixelType == PixelType::Float32) {
		desc.Format = Format::R32G32B32A32_SFLOAT;
	} else {
		desc.Format = texture.ColorSpace == ColorSpace::SRGB ? Format::R8G8B8A8_SRGB : Format::R8G8B8A8_UNORM;
	}
	desc.Usage = TextureUsage::Sampled;
	desc.GenerateMips = texture.Width > 1 || texture.Height > 1;
	desc.DebugName = texture.Name.c_str();

	Iunique<RHITexture> gpuTexture = RHITexture::Create(m_Device, desc, texture.Pixels.data());
	auto [it, inserted] = m_Textures.emplace(handle, std::move(gpuTexture));
	return *it->second;
}

} // namespace VKE
