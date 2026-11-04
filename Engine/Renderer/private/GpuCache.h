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

namespace VKE {
class AssetManager;
} // namespace VKE

namespace VKE {
class RHIDevice;
} // namespace VKE

namespace VKE {

struct GpuMesh {
	Iunique<RHIBuffer> VertexBuffer;
	Iunique<RHIBuffer> IndexBuffer;
	std::vector<Submesh> Submeshes;
	std::vector<Handle<MaterialData>> Materials;
	// One per submesh, in the mesh's own space.
	std::vector<AABB> SubmeshBounds;
};

struct GpuMaterial {
	Iunique<RHIBuffer> UniformBuffer;
	Iunique<RHIBindGroup> BindGroup;
	// The MaterialData::Revision it was made from.
	u32 Revision = 0;
};

// GPU copies of assets, created the first time each asset is requested. A material is made
// again when its data changes; the old copy is kept until the frames that used it are done.
class GpuCache {
public:
	GpuCache(RHIDevice &device, AssetManager &assets, RHIBindGroupLayout &materialLayout, u32 framesInFlight);
	~GpuCache();

	// Called as each frame begins, with the count of frames begun so far: frees the old
	// copies no frame in flight can still use.
	void BeginFrame(u64 frameNumber);

	// nullptr if the mesh asset doesn't exist.
	const GpuMesh *GetMesh(Handle<MeshData> handle);

	// Uses a default material if the asset doesn't exist.
	const GpuMaterial &GetMaterial(Handle<MaterialData> handle);

	// The lighting maps for an environment image, baked the first time they are
	// asked for. An invalid handle, or one whose asset is gone, gives the stand-in sky.
	const GpuEnvironment &GetEnvironmentMaps(Handle<TextureData> source);

	// Uploaded the first time it is asked for. A handle whose asset is gone gives white.
	RHITexture &GetTexture(Handle<TextureData> handle);

private:
	struct RetiredMaterial {
		u64 Frame = 0;
		Iunique<GpuMaterial> Material;
	};

	Iunique<GpuMaterial> CreateMaterial(const MaterialData &material);

	RHIDevice &m_Device;
	u32 m_FramesInFlight = 1;
	u64 m_FrameNumber = 0;
	AssetManager &m_Assets;
	RHIBindGroupLayout &m_MaterialLayout;

	Iunique<RHISampler> m_Sampler;
	Iunique<EnvironmentBaker> m_EnvironmentBaker;
	Handle<MaterialData> m_DefaultMaterial;

	// Destroyed in reverse declaration order: meshes, then materials, then textures.
	std::unordered_map<Handle<TextureData>, Iunique<RHITexture>> m_Textures;
	std::unordered_map<Handle<MaterialData>, Iunique<GpuMaterial>> m_Materials;
	std::vector<RetiredMaterial> m_RetiredMaterials;
	std::unordered_map<Handle<MeshData>, Iunique<GpuMesh>> m_Meshes;
	std::unordered_map<Handle<TextureData>, Iunique<GpuEnvironment>> m_Environments;
};

} // namespace VKE
