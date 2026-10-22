#include "scene/components/TransformComponent.h"
#include "scene/GameObject.h"

namespace VKE::Scene {

    void TransformComponent::SetLocal(const Transform& local) {
        m_Local = local;
        MarkChanged();
    }

    void TransformComponent::SetLocalPosition(const Vec3& position) {
        m_Local.Position = position;
        MarkChanged();
    }

    void TransformComponent::SetLocalRotation(const Quat& rotation) {
        m_Local.Rotation = rotation;
        MarkChanged();
    }

    void TransformComponent::SetLocalScale(const Vec3& scale) {
        m_Local.Scale = scale;
        MarkChanged();
    }

    const Transform& TransformComponent::GetWorld() const {
        UpdateWorld();
        return m_World;
    }

    void TransformComponent::SetWorldPosition(const Vec3& position) {
        const TransformComponent* parent = GetParentTransform();
        SetLocalPosition(parent ? Vec3(glm::inverse(parent->GetWorldMatrix()) * Vec4(position, 1.0f)) : position);
    }

    void TransformComponent::SetWorldRotation(const Quat& rotation) {
        const TransformComponent* parent = GetParentTransform();
        SetLocalRotation(parent ? glm::inverse(parent->GetWorld().Rotation) * rotation : rotation);
    }

    const Mat4& TransformComponent::GetWorldMatrix() const {
        UpdateWorld();
        return m_WorldMatrix;
    }

    void TransformComponent::UpdateWorld() const {
        if (!m_WorldDirty) return;

        Mat4 localMatrix = m_Local.GetMatrix();
        if (const TransformComponent* parentTransform = GetParentTransform()) {
            const Transform& parent = parentTransform->GetWorld();
            const Mat4& parentMatrix = parentTransform->GetWorldMatrix();
            m_World.Position = Vec3(parentMatrix * Vec4(m_Local.Position, 1.0f));
            // Composed directly instead of read back from the matrix, so it stays exact.
            m_World.Rotation = glm::normalize(parent.Rotation * m_Local.Rotation);
            m_World.Scale = parent.Scale * m_Local.Scale;
            m_WorldMatrix = parentMatrix * localMatrix;
        } else {
            m_World = m_Local;
            m_World.Rotation = glm::normalize(m_Local.Rotation);
            m_WorldMatrix = localMatrix;
        }
        m_WorldDirty = false;
    }

    const TransformComponent* TransformComponent::GetParentTransform() const {
        GameObject* parent = GetOwner().GetParent();
        return parent ? &parent->GetTransform() : nullptr;
    }

    void TransformComponent::MarkChanged() {
        bool wasDirty = m_WorldDirty;
        m_WorldDirty = true;
        bool queuedNow = MarkSyncNeeded();
        // A stale, queued transform always has stale, queued children too.
        if (wasDirty && !queuedNow) return;

        for (GameObject* child : GetOwner().GetChildren()) {
            child->GetTransform().MarkChanged();
        }
    }

} // namespace VKE::Scene
