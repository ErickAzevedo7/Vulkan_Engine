#pragma once

#include <assets/AssetTypes.h>
#include <core/Handle.h>
#include <core/Types.h>
#include <physics/PhysicsTypes.h>

#include <unordered_map>

namespace VKE::Physics {
    class PhysicsWorld;
} // namespace VKE::Physics

namespace VKE::Scene {

    // The physics shapes of the meshes a scene's colliders use, one per mesh however many
    // colliders share it, so each triangle tree is built once.
    class MeshShapeCache {
    public:
        explicit MeshShapeCache(Physics::PhysicsWorld& world) : m_World(world) {}

        // The shape of `mesh`, built from `geometry` the first time the mesh is asked for.
        Physics::MeshShapeID Acquire(Handle<Assets::MeshData> mesh, const Physics::MeshShapeDesc& geometry);
        // Destroys the shape once no collider uses it.
        void Release(Handle<Assets::MeshData> mesh);

    private:
        struct Entry {
            Physics::MeshShapeID Shape;
            u32 Users = 0;
        };

        Physics::PhysicsWorld& m_World;
        std::unordered_map<Handle<Assets::MeshData>, Entry> m_Entries;
    };

} // namespace VKE::Scene
