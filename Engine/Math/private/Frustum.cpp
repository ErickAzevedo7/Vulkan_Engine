#include <math/Frustum.h>

namespace VKE {

Frustum::Frustum(const Mat4 &viewProjection) {
	// glm stores columns; the planes come from the rows.
	const Mat4 rows = glm::transpose(viewProjection);
	Planes = {
		rows[3] + rows[0], // left
		rows[3] - rows[0], // right
		rows[3] + rows[1], // bottom
		rows[3] - rows[1], // top
		rows[2], // near, where depth is 0
		rows[3] - rows[2], // far
	};
}

bool Frustum::Intersects(const AABB &bounds) const {
	for (const Vec4 &plane : Planes) {
		// The box's corner furthest along the plane's normal: if it is outside, all of the box is.
		Vec3 corner(plane.x >= 0.0f ? bounds.Max.x : bounds.Min.x, plane.y >= 0.0f ? bounds.Max.y : bounds.Min.y,
				plane.z >= 0.0f ? bounds.Max.z : bounds.Min.z);
		if (glm::dot(Vec3(plane), corner) + plane.w < 0.0f) {
			return false;
		}
	}
	return true;
}

} // namespace VKE
