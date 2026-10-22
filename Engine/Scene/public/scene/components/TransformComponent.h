#pragma once

#include <scene/Component.h>

#include <math/MathTypes.h>
#include <math/Transform.h>

namespace VKE::Scene {

    // Where an object is. Every object has exactly one, created with the object and listed
    // first among its components; it can't be removed. The hierarchy belongs to GameObject;
    // the transform reads the parent from its owner to compute world values.
    //
    // Every accessor names its space. Local values are relative to the parent; they are what
    // the object stores. World values are derived from the local ones and every parent's, and
    // are the same as the local ones for an object without a parent.
    //
    // Both spaces are read as a Transform (GetLocal().Position, GetWorld().GetForward(), ...)
    // and written through setters, which keep the world values up to date.
    class TransformComponent final : public Component {
    public:
        // Created by GameObject; AddComponent refuses a second one.
        TransformComponent() = default;

        const Transform& GetLocal() const { return m_Local; }
        void SetLocal(const Transform& local);
        void SetLocalPosition(const Vec3& position);
        void SetLocalRotation(const Quat& rotation);
        void SetLocalScale(const Vec3& scale);

        // The world scale is exact unless a parent has a non-uniform scale and the object is
        // rotated under it, which skews the object: only the world matrix can hold the skew.
        // There is no world scale setter for the same reason.
        const Transform& GetWorld() const;
        void SetWorldPosition(const Vec3& position);
        void SetWorldRotation(const Quat& rotation);
        // Not the same as GetWorld().GetMatrix(): this one keeps the skew.
        const Mat4& GetWorldMatrix() const;

    private:
        // Recomputes the world values if a change made them stale.
        void UpdateWorld() const;

        // The parent's transform, or null for a root.
        const TransformComponent* GetParentTransform() const;

        // Marks the world values of this transform and its children as stale, and has their
        // objects synced at the end of the scene update.
        void MarkChanged();

        Transform m_Local;
        // Recomputed on the first read after a change, which can happen inside const getters.
        mutable Transform m_World;
        mutable Mat4 m_WorldMatrix{1.0f};
        mutable bool m_WorldDirty = true;
    };

} // namespace VKE::Scene
