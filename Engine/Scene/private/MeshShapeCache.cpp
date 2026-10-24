#include "MeshShapeCache.h"

#include <physics/PhysicsWorld.h>

namespace VKE::Scene {

    Physics::MeshShapeID MeshShapeCache::Acquire(Handle<Assets::MeshData> mesh, const Physics::MeshShapeDesc& geometry) {
        Entry& entry = m_Entries[mesh];
        if (entry.Users++ == 0) entry.Shape = m_World.CreateMeshShape(geometry);
        return entry.Shape;
    }

    void MeshShapeCache::Release(Handle<Assets::MeshData> mesh) {
        auto it = m_Entries.find(mesh);
        if (it == m_Entries.end() || --it->second.Users > 0) return;

        m_World.DestroyMeshShape(it->second.Shape);
        m_Entries.erase(it);
    }

} // namespace VKE::Scene
