#include "renderer/RenderScene.h"

#include <utility>

namespace VKE::Renderer {

    MeshInstanceID RenderScene::CreateMesh(const MeshInstanceDesc& desc) {
        return m_Meshes.Emplace(desc);
    }

    void RenderScene::SetTransform(MeshInstanceID id, const Mat4& transform) {
        if (MeshInstanceDesc* mesh = m_Meshes.Get(id)) mesh->Transform = transform;
    }

    void RenderScene::SetMaterials(MeshInstanceID id, std::vector<Handle<Assets::MaterialData>> materials) {
        if (MeshInstanceDesc* mesh = m_Meshes.Get(id)) mesh->Materials = std::move(materials);
    }

    void RenderScene::SetVisible(MeshInstanceID id, bool visible) {
        if (MeshInstanceDesc* mesh = m_Meshes.Get(id)) mesh->Visible = visible;
    }

    void RenderScene::DestroyMesh(MeshInstanceID id) {
        m_Meshes.Remove(id);
    }

    LightID RenderScene::CreateLight(const LightDesc& desc) {
        return m_Lights.Emplace(desc);
    }

    void RenderScene::SetLight(LightID id, const LightDesc& desc) {
        if (LightDesc* light = m_Lights.Get(id)) *light = desc;
    }

    void RenderScene::DestroyLight(LightID id) {
        m_Lights.Remove(id);
    }

} // namespace VKE::Renderer
