#pragma once

#include "EnvironmentBaker.h"

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/Types.h>
#include <math/AABB.h>
#include <rhi/RHIBindGroup.h>
#include <rhi/RHIBuffer.h>
#include <rhi/RHISampler.h>
#include <rhi/RHITexture.h>
#include <unordered_map>
#include <vector>

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::RHI {
    class RHIDevice;
} // namespace VKE::RHI

namespace VKE::Renderer {

    struct GpuMesh {
        Iunique<RHI::RHIBuffer> VertexBuffer;
        Iunique<RHI::RHIBuffer> IndexBuffer;
        std::vector<Assets::Submesh> Submeshes;
        std::vector<Handle<Assets::MaterialData>> Materials;
        // One per submesh, in the mesh's own space.
        std::vector<AABB> SubmeshBounds;
    };

    struct GpuMaterial {
        Iunique<RHI::RHIBuffer> UniformBuffer;
        Iunique<RHI::RHIBindGroup> BindGroup;
    };

    // GPU copies of assets, created the first time each asset is requested.
    class GpuCache {
    public:
        GpuCache(RHI::RHIDevice& device, Assets::AssetManager& assets, RHI::RHIBindGroupLayout& materialLayout);
        ~GpuCache();

        // nullptr if the mesh asset doesn't exist.
        const GpuMesh* GetMesh(Handle<Assets::MeshData> handle);

        // Uses a default material if the asset doesn't exist.
        const GpuMaterial& GetMaterial(Handle<Assets::MaterialData> handle);

        // The lighting maps for an environment image, baked the first time they are
        // asked for. An invalid handle, or one whose asset is gone, gives the stand-in sky.
        const GpuEnvironment& GetEnvironmentMaps(Handle<Assets::TextureData> source);

        // Uploaded the first time it is asked for. A handle whose asset is gone gives white.
        RHI::RHITexture& GetTexture(Handle<Assets::TextureData> handle);

    private:

        RHI::RHIDevice& m_Device;
        Assets::AssetManager& m_Assets;
        RHI::RHIBindGroupLayout& m_MaterialLayout;

        Iunique<RHI::RHISampler> m_Sampler;
        Iunique<EnvironmentBaker> m_EnvironmentBaker;
        Handle<Assets::MaterialData> m_DefaultMaterial;

        // Destroyed in reverse declaration order: meshes, then materials, then textures.
        std::unordered_map<Handle<Assets::TextureData>, Iunique<RHI::RHITexture>> m_Textures;
        std::unordered_map<Handle<Assets::MaterialData>, Iunique<GpuMaterial>> m_Materials;
        std::unordered_map<Handle<Assets::MeshData>, Iunique<GpuMesh>> m_Meshes;
        std::unordered_map<Handle<Assets::TextureData>, Iunique<GpuEnvironment>> m_Environments;
    };

} // namespace VKE::Renderer
