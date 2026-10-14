#include "GpuCache.h"
#include "ShaderLayouts.h"

#include <assets/AssetManager.h>
#include <core/Log.h>
#include <rhi/RHIDevice.h>

#include <cfloat>

namespace VKE::Renderer {

    static_assert(kMaterialUniformsBinding == static_cast<u32>(Assets::MaterialTexture::Count),
                  "Material uniforms must follow the texture bindings");

    GpuCache::GpuCache(RHI::RHIDevice& device, Assets::AssetManager& assets, RHI::RHIBindGroupLayout& materialLayout)
        : m_Device(device), m_Assets(assets), m_MaterialLayout(materialLayout) {
        RHI::SamplerDesc samplerDesc;
        samplerDesc.DebugName = "MaterialSampler";
        m_Sampler = RHI::RHISampler::Create(m_Device, samplerDesc);

        m_EnvironmentBaker = MakeIunique<EnvironmentBaker>(m_Device);

        Assets::MaterialData defaultMaterial;
        defaultMaterial.Name = "$default";
        defaultMaterial.Metallic = 0.0f;
        m_DefaultMaterial = m_Assets.AddMaterial(defaultMaterial);
    }

    GpuCache::~GpuCache() = default;

    const GpuMesh* GpuCache::GetMesh(Handle<Assets::MeshData> handle) {
        if (auto it = m_Meshes.find(handle); it != m_Meshes.end()) {
            return it->second.get();
        }

        const Assets::MeshData* mesh = m_Assets.GetMesh(handle);
        if (!mesh || mesh->Vertices.empty() || mesh->Indices.empty()) {
            return nullptr;
        }

        auto gpuMesh = MakeIunique<GpuMesh>();

        RHI::BufferDesc vertexDesc;
        vertexDesc.Size = mesh->Vertices.size() * sizeof(Assets::Vertex);
        vertexDesc.Usage = RHI::BufferUsage::Vertex;
        vertexDesc.DebugName = mesh->Name.c_str();
        gpuMesh->VertexBuffer = RHI::RHIBuffer::Create(m_Device, vertexDesc, mesh->Vertices.data());

        RHI::BufferDesc indexDesc;
        indexDesc.Size = mesh->Indices.size() * sizeof(u32);
        indexDesc.Usage = RHI::BufferUsage::Index;
        indexDesc.DebugName = mesh->Name.c_str();
        gpuMesh->IndexBuffer = RHI::RHIBuffer::Create(m_Device, indexDesc, mesh->Indices.data());

        gpuMesh->Submeshes = mesh->Submeshes;
        gpuMesh->Materials = mesh->Materials;
        for (const Assets::Submesh& submesh : mesh->Submeshes) {
            AABB bounds(Vec3(FLT_MAX), Vec3(-FLT_MAX));
            for (u32 i = submesh.FirstIndex; i < submesh.FirstIndex + submesh.IndexCount; ++i) {
                const Vec3& position = mesh->Vertices[mesh->Indices[i]].Position;
                bounds.Min = glm::min(bounds.Min, position);
                bounds.Max = glm::max(bounds.Max, position);
            }
            gpuMesh->SubmeshBounds.push_back(bounds);
        }

        auto [it, inserted] = m_Meshes.emplace(handle, std::move(gpuMesh));
        return it->second.get();
    }

    const GpuMaterial& GpuCache::GetMaterial(Handle<Assets::MaterialData> handle) {
        if (!m_Assets.GetMaterial(handle)) {
            handle = m_DefaultMaterial;
        }
        if (auto it = m_Materials.find(handle); it != m_Materials.end()) {
            return *it->second;
        }

        const Assets::MaterialData& material = *m_Assets.GetMaterial(handle);
        auto gpuMaterial = MakeIunique<GpuMaterial>();

        MaterialUniforms uniforms;
        uniforms.BaseColor = material.BaseColor;
        uniforms.EmissiveAndOcclusion = Vec4(material.Emissive, material.Occlusion);
        uniforms.MetallicRoughness = Vec4(material.Metallic, material.Roughness, 0.0f, 0.0f);

        RHI::BufferDesc uniformDesc;
        uniformDesc.Size = sizeof(MaterialUniforms);
        uniformDesc.Usage = RHI::BufferUsage::Uniform;
        uniformDesc.DebugName = material.Name.c_str();
        gpuMaterial->UniformBuffer = RHI::RHIBuffer::Create(m_Device, uniformDesc, &uniforms);

        RHI::BindGroupDesc groupDesc;
        groupDesc.Layout = &m_MaterialLayout;
        groupDesc.DebugName = material.Name.c_str();
        for (u32 slot = 0; slot < static_cast<u32>(Assets::MaterialTexture::Count); ++slot) {
            RHI::BoundResource resource;
            resource.Binding = slot;
            resource.Texture = &GetTexture(material.Textures[slot]);
            resource.Sampler = m_Sampler.get();
            groupDesc.Resources.push_back(resource);
        }

        RHI::BoundResource uniformResource;
        uniformResource.Binding = kMaterialUniformsBinding;
        uniformResource.Buffer = gpuMaterial->UniformBuffer.get();
        groupDesc.Resources.push_back(uniformResource);

        gpuMaterial->BindGroup = RHI::RHIBindGroup::Create(m_Device, groupDesc);

        auto [it, inserted] = m_Materials.emplace(handle, std::move(gpuMaterial));
        return *it->second;
    }

    const GpuEnvironment& GpuCache::GetEnvironmentMaps(Handle<Assets::TextureData> source) {
        if (source.IsValid() && !m_Assets.GetTexture(source)) {
            source = {};
        }
        if (auto it = m_Environments.find(source); it != m_Environments.end()) {
            return *it->second;
        }

        RHI::RHITexture* sourceTexture = source.IsValid() ? &GetTexture(source) : nullptr;
        auto [it, inserted] = m_Environments.emplace(source, m_EnvironmentBaker->Bake(sourceTexture));
        return *it->second;
    }

    RHI::RHITexture& GpuCache::GetTexture(Handle<Assets::TextureData> handle) {
        if (!m_Assets.GetTexture(handle)) {
            handle = m_Assets.GetDefaultTexture(Assets::DefaultTexture::White);
        }
        if (auto it = m_Textures.find(handle); it != m_Textures.end()) {
            return *it->second;
        }

        const Assets::TextureData& texture = *m_Assets.GetTexture(handle);

        RHI::TextureDesc desc;
        desc.Width = texture.Width;
        desc.Height = texture.Height;
        if (texture.PixelType == Assets::PixelType::Float32) {
            desc.Format = RHI::Format::R32G32B32A32_SFLOAT;
        } else {
            desc.Format = texture.ColorSpace == Assets::ColorSpace::SRGB ? RHI::Format::R8G8B8A8_SRGB
                                                                         : RHI::Format::R8G8B8A8_UNORM;
        }
        desc.Usage = RHI::TextureUsage::Sampled;
        desc.GenerateMips = texture.Width > 1 || texture.Height > 1;
        desc.DebugName = texture.Name.c_str();

        Iunique<RHI::RHITexture> gpuTexture = RHI::RHITexture::Create(m_Device, desc, texture.Pixels.data());
        auto [it, inserted] = m_Textures.emplace(handle, std::move(gpuTexture));
        return *it->second;
    }

} // namespace VKE::Renderer
