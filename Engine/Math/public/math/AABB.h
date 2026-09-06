#pragma once

#include <math/MathTypes.h>
#include <vector>

namespace VKE {
    struct AABB {
        Vec3 Min{0.0f, 0.0f, 0.0f};
        Vec3 Max{0.0f, 0.0f, 0.0f};

        AABB() = default;
        AABB(const Vec3& min, const Vec3& max) : Min(min), Max(max) {}

        bool Contains(const Vec3& point) const;
        bool Intersects(const AABB& other) const;
        
        Vec3 GetCenter() const;
        Vec3 GetExtents() const;
        Vec3 GetSize() const;

        void Merge(const AABB& other);

        static AABB FromPoints(const std::vector<Vec3>& points);
    };
}
