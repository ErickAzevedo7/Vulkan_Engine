#pragma once

#include <scene/Component.h>

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/Types.h>
#include <math/MathTypes.h>
#include <physics/PhysicsTypes.h>

namespace VKE::Assets {
    class AssetManager;
} // namespace VKE::Assets

namespace VKE::Scene {

    // Gives its object a shape that collides with other colliders. Starts as a static, solid
    // 1×1×1 box. A movable collider is pushed out of the solid ones it overlaps, which moves its
    // object; a trigger only reports overlaps. What touched what is in Scene::GetContacts.
    // Leaves the physics world while its object is inactive.
    class ColliderComponent final : public Component {
    public:
        ColliderComponent() = default;

        Physics::ShapeType GetShape() const { return m_Shape; }

        // A box in the object's own space, so it follows the object's rotation and scale.
        void SetBox(const Vec3& center, const Vec3& halfExtents);
        const Vec3& GetBoxCenter() const { return m_BoxCenter; }
        const Vec3& GetBoxHalfExtents() const { return m_BoxHalfExtents; }

        // The triangles of `mesh`, placed like a mesh component on the same object would draw them.
        // Read from `assets` now. Mesh colliders are always static.
        void SetMesh(const Assets::AssetManager& assets, Handle<Assets::MeshData> mesh);
        Handle<Assets::MeshData> GetMesh() const { return m_Mesh; }

        Physics::BodyMotion GetMotion() const { return m_Motion; }
        void SetMotion(Physics::BodyMotion motion);

        bool IsTrigger() const { return m_IsTrigger; }
        void SetTrigger(bool isTrigger);

    private:
        void ConnectToSystems(SceneSystems& systems) override;
        void SyncToSystems(SceneSystems& systems) override;
        void DisconnectFromSystems(SceneSystems& systems) override;

        void ReleaseMeshShape(SceneSystems& systems);
        void DestroyBody(SceneSystems& systems);

        Physics::ShapeType m_Shape = Physics::ShapeType::Box;
        Vec3 m_BoxCenter{0.0f};
        Vec3 m_BoxHalfExtents{0.5f};
        Handle<Assets::MeshData> m_Mesh;
        Physics::BodyMotion m_Motion = Physics::BodyMotion::Static;
        bool m_IsTrigger = false;

        // Triangles read by SetMesh, until the next sync turns them into a shape.
        Iunique<Physics::MeshShapeDesc> m_PendingGeometry;
        // The mesh whose shape this collider holds in the scene's cache, if any.
        Handle<Assets::MeshData> m_ShapeMesh;
        Physics::MeshShapeID m_MeshShape;
        Physics::BodyID m_Body;
    };

} // namespace VKE::Scene
