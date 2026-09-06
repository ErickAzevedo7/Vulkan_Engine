#include <math/AABB.h>
#include <math/MathUtils.h>

namespace VKE {
    bool AABB::Contains(const Vec3& point) const {
        return point.x >= Min.x && point.x <= Max.x &&
               point.y >= Min.y && point.y <= Max.y &&
               point.z >= Min.z && point.z <= Max.z;
    }

    bool AABB::Intersects(const AABB& other) const {
        return Min.x <= other.Max.x && Max.x >= other.Min.x &&
               Min.y <= other.Max.y && Max.y >= other.Min.y &&
               Min.z <= other.Max.z && Max.z >= other.Min.z;
    }

    Vec3 AABB::GetCenter() const {
        return (Min + Max) * 0.5f;
    }

    Vec3 AABB::GetExtents() const {
        return (Max - Min) * 0.5f;
    }

    Vec3 AABB::GetSize() const {
        return Max - Min;
    }

    void AABB::Merge(const AABB& other) {
        Min = glm::min(Min, other.Min);
        Max = glm::max(Max, other.Max);
    }

    AABB AABB::FromPoints(const std::vector<Vec3>& points) {
        if (points.empty()) return AABB();

        AABB result(points[0], points[0]);
        for (size_t i = 1; i < points.size(); ++i) {
            result.Min = glm::min(result.Min, points[i]);
            result.Max = glm::max(result.Max, points[i]);
        }
        return result;
    }
}
