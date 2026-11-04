#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/Types.h>
#include <physics/PhysicsTypes.h>

#include <unordered_map>

namespace VKE {
class PhysicsWorld;
} // namespace VKE

namespace VKE {

// The physics shapes of the meshes a scene's colliders use, one per mesh however many
// colliders share it, so each triangle tree is built once.
class MeshShapeCache {
public:
	explicit MeshShapeCache(PhysicsWorld &world) : m_World(world) {}

	// The shape of `mesh`, built from `geometry` the first time the mesh is asked for.
	MeshShapeID Acquire(Handle<MeshData> mesh, const MeshShapeDesc &geometry);
	// Destroys the shape once no collider uses it.
	void Release(Handle<MeshData> mesh);

private:
	struct Entry {
		MeshShapeID Shape;
		u32 Users = 0;
	};

	PhysicsWorld &m_World;
	std::unordered_map<Handle<MeshData>, Entry> m_Entries;
};

} // namespace VKE
