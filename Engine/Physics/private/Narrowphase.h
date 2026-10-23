#pragma once

#include "TriangleTree.h"

#include <core/Types.h>
#include <math/AABB.h>
#include <math/MathTypes.h>

namespace VKE::Physics {

    // A box in world space: center, unit axes and half the size along each axis.
    struct OrientedBox {
        Vec3 Center{0.0f};
        Vec3 Axes[3] = {Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)};
        Vec3 HalfExtents{0.5f};
    };

    // How far two shapes overlap along the axis where they overlap least, found by the
    // separating axis test.
    struct Penetration {
        // Moves the first shape out of the second.
        Vec3 Normal{0.0f};
        // Positive when the shapes overlap; down to -margin when they only come that close.
        f32 Depth = 0.0f;
    };

    AABB GetBounds(const OrientedBox& box);

    // Each returns false when the shapes are farther apart than `margin`.
    bool TestBoxBox(const OrientedBox& a, const OrientedBox& b, f32 margin, Penetration& out);
    bool TestBoxTriangle(const OrientedBox& box, const Triangle& triangle, f32 margin, Penetration& out);

} // namespace VKE::Physics
