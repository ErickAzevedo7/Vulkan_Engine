#pragma once

#include <math/AABB.h>
#include <math/MathTypes.h>

#include <array>

namespace VKE {

// What a view-projection matrix sees, as six planes facing inward. Expects clip space with depth in
// [0, 1], as the renderer's projections give.
struct Frustum {
	// xyz is the plane's normal and w its offset: a point p is on the inside when dot(xyz, p) + w >= 0.
	std::array<Vec4, 6> Planes{};

	Frustum() = default;
	explicit Frustum(const Mat4 &viewProjection);

	// Conservative: a box just outside a corner can pass, but a box that is seen never fails.
	bool Intersects(const AABB &bounds) const;
};

} // namespace VKE
