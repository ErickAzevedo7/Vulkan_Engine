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

} // namespace VKE::Scene
