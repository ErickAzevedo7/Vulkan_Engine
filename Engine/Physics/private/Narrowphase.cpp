#include "Narrowphase.h"

#include <cfloat>
#include <cmath>

namespace VKE::Physics {

    namespace {
        // Cross products shorter than this come from parallel edges and give no axis.
        constexpr f32 kMinAxisLength = 1e-6f;

        // Pushing a box out of a mesh along the triangle's own normal keeps it from catching on
        // the edges between neighboring triangles of a flat surface, which would shove it
        // sideways. Another axis is used only when it pushes this much less.
        constexpr f32 kFaceAxisPreference = 0.05f;

        f32 ProjectBox(const OrientedBox& box, const Vec3& axis) {
            return box.HalfExtents.x * std::abs(glm::dot(box.Axes[0], axis)) +
                   box.HalfExtents.y * std::abs(glm::dot(box.Axes[1], axis)) +
                   box.HalfExtents.z * std::abs(glm::dot(box.Axes[2], axis));
        }

        // Runs the separating axis test one axis at a time and keeps the one of least overlap.
        class AxisSearch {
        public:
            explicit AxisSearch(f32 margin) : m_Margin(margin) {}

            // Projections of the first shape [minA, maxA] and the second [minB, maxB] onto the unit
            // `axis`. Returns false when the shapes are separated along it, which ends the test.
            bool Test(const Vec3& axis, f32 minA, f32 maxA, f32 minB, f32 maxB) {
                // Distance to move the first shape along +axis, or along -axis, to clear the second.
                f32 forward = maxB - minA;
                f32 backward = maxA - minB;
                f32 depth = std::min(forward, backward);
                if (depth < -m_Margin) return false;

                if (depth < m_Best.Depth) {
                    m_Best.Depth = depth;
                    m_Best.Normal = forward < backward ? axis : -axis;
                }
                return true;
            }

            const Penetration& GetBest() const { return m_Best; }

        private:
            f32 m_Margin;
            Penetration m_Best{Vec3(0.0f), FLT_MAX};
        };

        bool TestBoxAxis(AxisSearch& search, const OrientedBox& box, const Vec3& axis, f32 centerB, f32 radiusB) {
            f32 center = glm::dot(box.Center, axis);
            f32 radius = ProjectBox(box, axis);
            return search.Test(axis, center - radius, center + radius, centerB - radiusB, centerB + radiusB);
        }
    } // namespace

    AABB GetBounds(const OrientedBox& box) {
        Vec3 extent(0.0f);
        for (int i = 0; i < 3; ++i) {
            extent += glm::abs(box.Axes[i]) * box.HalfExtents[i];
        }
        return AABB(box.Center - extent, box.Center + extent);
    }

    bool TestBoxBox(const OrientedBox& a, const OrientedBox& b, f32 margin, Penetration& out) {
        AxisSearch search(margin);
        auto test = [&](const Vec3& axis) {
            return TestBoxAxis(search, a, axis, glm::dot(b.Center, axis), ProjectBox(b, axis));
        };

        for (int i = 0; i < 3; ++i) {
            if (!test(a.Axes[i]) || !test(b.Axes[i])) return false;
        }
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                Vec3 axis = glm::cross(a.Axes[i], b.Axes[j]);
                f32 length = glm::length(axis);
                if (length < kMinAxisLength) continue;
                if (!test(axis / length)) return false;
            }
        }

        out = search.GetBest();
        return true;
    }

    bool TestBoxTriangle(const OrientedBox& box, const Triangle& triangle, f32 margin, Penetration& out) {
        const Vec3 edges[3] = {triangle.B - triangle.A, triangle.C - triangle.B, triangle.A - triangle.C};
        Vec3 normal = glm::cross(edges[0], -edges[2]);
        f32 normalLength = glm::length(normal);
        // A triangle with no area can't be touched.
        if (normalLength < kMinAxisLength) return false;
        normal /= normalLength;

        AxisSearch search(margin);
        auto test = [&](const Vec3& axis) {
            f32 a = glm::dot(triangle.A, axis);
            f32 b = glm::dot(triangle.B, axis);
            f32 c = glm::dot(triangle.C, axis);
            f32 low = std::min(a, std::min(b, c));
            f32 high = std::max(a, std::max(b, c));
            return TestBoxAxis(search, box, axis, 0.5f * (low + high), 0.5f * (high - low));
        };

        if (!test(normal)) return false;
        // The face axis is measured first, alone, so it can be preferred below.
        Penetration face = search.GetBest();

        for (int i = 0; i < 3; ++i) {
            if (!test(box.Axes[i])) return false;
        }
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                Vec3 axis = glm::cross(box.Axes[i], edges[j]);
                f32 length = glm::length(axis);
                if (length < kMinAxisLength) continue;
                if (!test(axis / length)) return false;
            }
        }

        // The triangle is flat along its normal, so the face axis pushes the box out toward the
        // side its center is on.
        const Penetration& best = search.GetBest();
        out = face.Depth <= best.Depth + kFaceAxisPreference ? face : best;
        return true;
    }

} // namespace VKE::Physics
