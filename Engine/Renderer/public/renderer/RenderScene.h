#pragma once

#include <core/SlotMap.h>
#include <renderer/RenderTypes.h>
#include <vector>

namespace VKE::Renderer {

    // What gets drawn: mesh instances and lights. Holds only data, so entries can
    // be created, changed and destroyed at any time.
    class RenderScene {
    public:
        MeshInstanceID CreateMesh(const MeshInstanceDesc& desc);
        void SetMesh(MeshInstanceID id, Handle<Assets::MeshData> mesh);
        void SetTransform(MeshInstanceID id, const Mat4& transform);
        void SetMaterials(MeshInstanceID id, std::vector<Handle<Assets::MaterialData>> materials);
        void SetVisible(MeshInstanceID id, bool visible);
        void DestroyMesh(MeshInstanceID id);

        void SetEnvironment(const EnvironmentDesc& desc) { m_Environment = desc; }
        const EnvironmentDesc& GetEnvironment() const { return m_Environment; }

        LightID CreateLight(const LightDesc& desc);
        void SetLight(LightID id, const LightDesc& desc);
        void DestroyLight(LightID id);

        const SlotMap<MeshInstanceDesc>& GetMeshes() const { return m_Meshes; }
        const SlotMap<LightDesc>& GetLights() const { return m_Lights; }

    private:
        SlotMap<MeshInstanceDesc> m_Meshes;
        SlotMap<LightDesc> m_Lights;
        EnvironmentDesc m_Environment;
    };

} // namespace VKE::Renderer
