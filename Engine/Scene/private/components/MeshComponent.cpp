#include "scene/components/MeshComponent.h"
#include "SceneSystems.h"
#include "scene/GameObject.h"

#include <utility>

namespace VKE::Scene {

    MeshComponent::MeshComponent(Handle<Assets::MeshData> mesh)
        : m_Mesh(mesh) {
    }

    void MeshComponent::SetMesh(Handle<Assets::MeshData> mesh) {
        m_Mesh = mesh;
        MarkSyncNeeded();
    }

    void MeshComponent::SetMaterials(std::vector<Handle<Assets::MaterialData>> materials) {
        m_Materials = std::move(materials);
        MarkSyncNeeded();
    }

    void MeshComponent::ConnectToSystems(SceneSystems& systems) {
        // Starts hidden; the first sync fills in the rest.
        Renderer::MeshInstanceDesc desc;
        desc.Visible = false;
        desc.Owner = PackObjectHandle(GetOwner().GetHandle());
        m_Instance = systems.Render.CreateMesh(desc);
        MarkSyncNeeded();
    }

    void MeshComponent::SyncToSystems(SceneSystems& systems) {
        systems.Render.SetMesh(m_Instance, m_Mesh);
        systems.Render.SetMaterials(m_Instance, m_Materials);
        systems.Render.SetTransform(m_Instance, GetTransform().GetWorldMatrix());
        systems.Render.SetVisible(m_Instance, GetOwner().IsActiveInHierarchy());
    }

    void MeshComponent::DisconnectFromSystems(SceneSystems& systems) {
        systems.Render.DestroyMesh(m_Instance);
        m_Instance = {};
    }

} // namespace VKE::Scene
