#pragma once

#include <core/Handle.h>
#include <core/Types.h>
#include <math/MathTypes.h>
#include <math/Transform.h>
#include <reflection/Marks.h>

#include <vector>

namespace VKE::Physics {

    // Triangles for mesh bodies, in the mesh's own space. One shape can back many bodies.
    struct MeshShapeDesc {
        std::vector<Vec3> Positions;
        // Three per triangle.
        std::vector<u32> Indices;
    };

    VKE_ENUM()
    enum class ShapeType : u8 {
        Box,
        // Always static: a mesh is never pushed.
        Mesh,
    };

    VKE_ENUM()
    enum class BodyMotion : u8 {
        // Never moves when hit; pushes movable bodies out of it.
        Static,
        // Pushed out of the solid bodies it overlaps.
        Movable,
    };

    // What the world stores for a mesh shape and a body. Defined only inside the physics module;
    // outside it they just name the handles.
    struct MeshShape;
    struct Body;

    using MeshShapeID = Handle<MeshShape>;

    struct BodyDesc {
        ShapeType Shape = ShapeType::Box;
        // Box bodies, in the body's own space, so they follow its rotation and scale.
        Vec3 Center{0.0f};
        Vec3 HalfExtents{0.5f};
        // Mesh bodies.
        MeshShapeID Mesh;
        // Where the body is in the world.
        Transform Pose;
        BodyMotion Motion = BodyMotion::Static;
        // A trigger reports overlaps but never pushes or gets pushed.
        bool IsTrigger = false;
        // Not used by the world; copied into the body's contact events, so the owner can tell what
        // the body belongs to even after it is destroyed.
        u64 UserData = 0;
    };

    using BodyID = Handle<Body>;

    // Where a ray first met a body.
    struct RayHit {
        BodyID Body;
        Vec3 Point{0.0f};
        // Against the ray.
        Vec3 Normal{0.0f};
        f32 Distance = 0.0f;
        // BodyDesc::UserData.
        u64 UserData = 0;
    };

    enum class ContactPhase : u8 {
        // The bodies started touching in this step.
        Begin,
        // They were touching in the last step too.
        Stay,
        // They stopped touching, one of them was destroyed, or the pair became or stopped being
        // a trigger pair (which ends it and begins it again).
        End,
    };

    struct ContactEvent {
        BodyID A;
        BodyID B;
        ContactPhase Phase = ContactPhase::Begin;
        // Either body is a trigger, so nothing was pushed.
        bool IsTrigger = false;
        // Solid contacts that haven't ended: the direction that moves A away from B.
        Vec3 Normal{0.0f};
        // BodyDesc::UserData of A and B.
        u64 UserDataA = 0;
        u64 UserDataB = 0;
    };

} // namespace VKE::Physics
