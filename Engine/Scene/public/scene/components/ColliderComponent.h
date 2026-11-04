#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/Types.h>
#include <math/MathTypes.h>
#include <physics/PhysicsTypes.h>
#include <scene/Component.h>

namespace VKE {
class AssetManager;
} // namespace VKE

namespace VKE {

// Gives its object a shape that collides with other colliders. Starts as a static, solid
// 1×1×1 box. A movable collider is pushed out of the solid ones it overlaps, which moves its
// object; a trigger only reports overlaps. What touched what is in Scene::GetContacts.
// Leaves the physics world while its object is inactive.
class ColliderComponent final : public Component {
	VKE_COMPONENT(ColliderComponent)

public:
	ColliderComponent() = default;

	ShapeType GetShape() const { return m_Shape; }

	// A box in the object's own space, so it follows the object's rotation and scale.
	void SetBox(const Vec3 &center, const Vec3 &halfExtents);
	const Vec3 &GetBoxCenter() const { return m_BoxCenter; }
	const Vec3 &GetBoxHalfExtents() const { return m_BoxHalfExtents; }

	// The triangles of `mesh`, placed like a mesh component on the same object would draw them.
	// Read from `assets` now. Mesh colliders are always static.
	void SetMesh(const AssetManager &assets, Handle<MeshData> mesh);
	Handle<MeshData> GetMesh() const { return m_Mesh; }

	BodyMotion GetMotion() const { return m_Motion; }
	void SetMotion(BodyMotion motion);

	bool IsTrigger() const { return m_IsTrigger; }
	void SetTrigger(bool isTrigger);

private:
	void ConnectToSystems(SceneSystems &systems) override;
	void SyncToSystems(SceneSystems &systems) override;
	void DisconnectFromSystems(SceneSystems &systems) override;
	void OnPropertiesChanged(const AssetManager &assets) override;

	void ReleaseMeshShape(SceneSystems &systems);
	void DestroyBody(SceneSystems &systems);

	VKE_PROPERTY() ShapeType m_Shape = ShapeType::Box;
	VKE_PROPERTY() Vec3 m_BoxCenter{ 0.0f };
	VKE_PROPERTY() Vec3 m_BoxHalfExtents{ 0.5f };
	VKE_PROPERTY() Handle<MeshData> m_Mesh;
	VKE_PROPERTY() BodyMotion m_Motion = BodyMotion::Static;
	VKE_PROPERTY() bool m_IsTrigger = false;

	// Triangles read by SetMesh, until the next sync turns them into a shape.
	Iunique<MeshShapeDesc> m_PendingGeometry;
	// The mesh whose shape this collider holds in the scene's cache, if any.
	Handle<MeshData> m_ShapeMesh;
	MeshShapeID m_MeshShape;
	BodyID m_Body;
};

} // namespace VKE
