#include "scene/Component.h"
#include "scene/GameObject.h"

namespace VKE::Scene {

    Scene& Component::GetScene() const {
        return m_Owner->GetScene();
    }

    TransformComponent& Component::GetTransform() const {
        return m_Owner->GetTransform();
    }

    bool Component::MarkSyncNeeded() {
        return m_Owner->MarkSyncNeeded();
    }

    void Component::ApplyPropertyChanges(const Assets::AssetManager& assets) {
        OnPropertiesChanged(assets);
    }

    void Component::OnPropertiesChanged(const Assets::AssetManager&) {
        MarkSyncNeeded();
    }

} // namespace VKE::Scene
