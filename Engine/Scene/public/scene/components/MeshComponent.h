#pragma once

#include <scene/Component.h>

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <renderer/RenderTypes.h>

#include <vector>

namespace VKE::Scene {

    // Draws a mesh where its object is.
    class MeshComponent final : public Component {
    public:
        explicit MeshComponent(Handle<Assets::MeshData> mesh = {});

        Handle<Assets::MeshData> GetMesh() const { return m_Mesh; }
        void SetMesh(Handle<Assets::MeshData> mesh);

        // Replace the mesh's materials, indexed like MeshData::Materials. Empty keeps the mesh's own.
        const std::vector<Handle<Assets::MaterialData>>& GetMaterials() const { return m_Materials; }
        void SetMaterials(std::vector<Handle<Assets::MaterialData>> materials);

    private:
        void ConnectToSystems(SceneSystems& systems) override;
        void SyncToSystems(SceneSystems& systems) override;
        void DisconnectFromSystems(SceneSystems& systems) override;

        Handle<Assets::MeshData> m_Mesh;
        std::vector<Handle<Assets::MaterialData>> m_Materials;
        Renderer::MeshInstanceID m_Instance;
    };

} // namespace VKE::Scene
