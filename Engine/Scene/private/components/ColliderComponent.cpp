#include "scene/components/ColliderComponent.h"
#include "SceneSystems.h"
#include "scene/GameObject.h"

#include <assets/AssetManager.h>
#include <core/Log.h>

namespace VKE::Scene {

    void ColliderComponent::SetBox(const Vec3& center, const Vec3& halfExtents) {
        m_Shape = Physics::ShapeType::Box;
        m_BoxCenter = center;
        m_BoxHalfExtents = halfExtents;
        m_Mesh = {};
        m_PendingGeometry.reset();
        MarkSyncNeeded();
    }

    void ColliderComponent::SetMesh(const Assets::AssetManager& assets, Handle<Assets::MeshData> mesh) {
        m_Shape = Physics::ShapeType::Mesh;
        m_Mesh = mesh;
        m_PendingGeometry = MakeIunique<Physics::MeshShapeDesc>();

        if (const Assets::MeshData* data = assets.GetMesh(mesh)) {
            m_PendingGeometry->Positions.reserve(data->Vertices.size());
            for (const Assets::Vertex& vertex : data->Vertices) {
                m_PendingGeometry->Positions.push_back(vertex.Position);
            }
            m_PendingGeometry->Indices = data->Indices;
        } else {
            VKE_CORE_WARN("ColliderComponent: '{}' has a mesh collider without a loaded mesh", GetOwner().GetName());
        }
        MarkSyncNeeded();
    }

    void ColliderComponent::SetMotion(Physics::BodyMotion motion) {
        m_Motion = motion;
        MarkSyncNeeded();
    }

    void ColliderComponent::SetTrigger(bool isTrigger) {
        m_IsTrigger = isTrigger;
        MarkSyncNeeded();
    }

    void ColliderComponent::OnPropertiesChanged(const Assets::AssetManager& assets) {
        // A new mesh needs its triangles read, which SetMesh does.
        if (m_Shape == Physics::ShapeType::Mesh && m_Mesh != m_ShapeMesh) {
            SetMesh(assets, m_Mesh);
        } else {
            MarkSyncNeeded();
        }
    }

    void ColliderComponent::ConnectToSystems(SceneSystems&) {
        // The body enters the physics world on the first sync, if its object is active.
        MarkSyncNeeded();
    }

    void ColliderComponent::SyncToSystems(SceneSystems& systems) {
        if (m_PendingGeometry) {
            // Taken before the old one is given back, so setting the same mesh again doesn't
            // rebuild its shape.
            Physics::MeshShapeID shape = systems.MeshShapes.Acquire(m_Mesh, *m_PendingGeometry);
            ReleaseMeshShape(systems);
            m_MeshShape = shape;
            m_ShapeMesh = m_Mesh;
            m_PendingGeometry.reset();
        } else if (m_Shape != Physics::ShapeType::Mesh) {
            ReleaseMeshShape(systems);
        }

        // The physics world has no disabled bodies, so an inactive collider leaves it.
        if (!GetOwner().IsActiveInHierarchy()) {
            DestroyBody(systems);
            return;
        }

        Physics::BodyDesc desc;
        desc.Shape = m_Shape;
        desc.Center = m_BoxCenter;
        desc.HalfExtents = m_BoxHalfExtents;
        desc.Mesh = m_MeshShape;
        desc.Pose = GetTransform().GetWorld();
        desc.Motion = m_Motion;
        desc.IsTrigger = m_IsTrigger;
        desc.UserData = PackObjectHandle(GetOwner().GetHandle());

        if (m_Body.IsValid()) {
            systems.Physics.SetBody(m_Body, desc);
        } else {
            m_Body = systems.Physics.CreateBody(desc);
        }
    }

    void ColliderComponent::DisconnectFromSystems(SceneSystems& systems) {
        DestroyBody(systems);
        ReleaseMeshShape(systems);
    }

    void ColliderComponent::ReleaseMeshShape(SceneSystems& systems) {
        if (m_ShapeMesh.IsValid()) systems.MeshShapes.Release(m_ShapeMesh);
        m_ShapeMesh = {};
        m_MeshShape = {};
    }

    void ColliderComponent::DestroyBody(SceneSystems& systems) {
        if (m_Body.IsValid()) systems.Physics.DestroyBody(m_Body);
        m_Body = {};
    }

} // namespace VKE::Scene
