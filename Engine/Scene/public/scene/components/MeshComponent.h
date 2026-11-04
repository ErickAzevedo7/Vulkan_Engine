#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <renderer/RenderTypes.h>
#include <scene/Component.h>

#include <vector>

namespace VKE {

// Draws a mesh where its object is.
class MeshComponent final : public Component {
	VKE_COMPONENT(MeshComponent)

public:
	explicit MeshComponent(Handle<MeshData> mesh = {});

	Handle<MeshData> GetMesh() const { return m_Mesh; }
	void SetMesh(Handle<MeshData> mesh);

	// Replace the mesh's materials, indexed like MeshData::Materials. Empty keeps the mesh's own.
	const std::vector<Handle<MaterialData>> &GetMaterials() const { return m_Materials; }
	void SetMaterials(std::vector<Handle<MaterialData>> materials);

private:
	void ConnectToSystems(SceneSystems &systems) override;
	void SyncToSystems(SceneSystems &systems) override;
	void DisconnectFromSystems(SceneSystems &systems) override;

	VKE_PROPERTY() Handle<MeshData> m_Mesh;
	VKE_PROPERTY() std::vector<Handle<MaterialData>> m_Materials;
	MeshInstanceID m_Instance;
};

} // namespace VKE
