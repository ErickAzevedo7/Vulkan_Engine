#include "scene/components/MeshComponent.h"

#include "SceneSystems.h"
#include "scene/GameObject.h"

#include <utility>

namespace VKE {

MeshComponent::MeshComponent(Handle<MeshData> mesh) : m_Mesh(mesh) {}

void MeshComponent::SetMesh(Handle<MeshData> mesh) {
	m_Mesh = mesh;
	MarkSyncNeeded();
}

void MeshComponent::SetMaterials(std::vector<Handle<MaterialData>> materials) {
	m_Materials = std::move(materials);
	MarkSyncNeeded();
}

void MeshComponent::ConnectToSystems(SceneSystems &systems) {
	// Starts hidden; the first sync fills in the rest.
	MeshInstanceDesc desc;
	desc.Visible = false;
	desc.Owner = PackObjectHandle(GetOwner().GetHandle());
	m_Instance = systems.Render.CreateMesh(desc);
	MarkSyncNeeded();
}

void MeshComponent::SyncToSystems(SceneSystems &systems) {
	systems.Render.SetMesh(m_Instance, m_Mesh);
	systems.Render.SetMaterials(m_Instance, m_Materials);
	systems.Render.SetTransform(m_Instance, GetTransform().GetWorldMatrix());
	systems.Render.SetVisible(m_Instance, GetOwner().IsActiveInHierarchy());
}

void MeshComponent::DisconnectFromSystems(SceneSystems &systems) {
	systems.Render.DestroyMesh(m_Instance);
	m_Instance = {};
}

} // namespace VKE
