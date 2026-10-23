#pragma once

#include <physics/PhysicsTypes.h>

#include <core/Types.h>
#include <math/Transform.h>

#include <vector>

namespace VKE::Physics {

    // Bodies that collide and push each other apart, without gravity, velocity or rotation.
    // Holds only data and is changed through handles, like the render scene.
    //
    // Each Step pushes movable bodies out of the solid bodies they overlap, then reports which
    // bodies moved and which pairs started, kept or stopped touching.
    class PhysicsWorld {
    public:
        PhysicsWorld();
        ~PhysicsWorld();

        PhysicsWorld(const PhysicsWorld&) = delete;
        PhysicsWorld& operator=(const PhysicsWorld&) = delete;

        // Builds the triangle tree once; bodies share it through the returned handle.
        MeshShapeID CreateMeshShape(const MeshShapeDesc& desc);
        // Bodies still using the shape stop colliding.
        void DestroyMeshShape(MeshShapeID id);

        BodyID CreateBody(const BodyDesc& desc);
        void SetBody(BodyID id, const BodyDesc& desc);
        // Moves the body there directly, without being pushed on the way.
        void SetPose(BodyID id, const Transform& pose);
        void DestroyBody(BodyID id);
        const BodyDesc* GetBody(BodyID id) const;

        void Step();

        // Results of the last Step. A moved body's new position is in GetBody(id)->Pose.
        const std::vector<BodyID>& GetMovedBodies() const;
        const std::vector<ContactEvent>& GetContactEvents() const;

    private:
        struct Storage;
        Iunique<Storage> m_Storage;
    };

} // namespace VKE::Physics
