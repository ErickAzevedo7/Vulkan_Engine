#include "TriangleTree.h"

#include <algorithm>

namespace VKE::Physics {

    namespace {
        bool RayTouchesBounds(const AABB& bounds, const Vec3& origin, const Vec3& direction, f32 maxT) {
            f32 near = 0.0f;
            f32 far = maxT;
            for (int axis = 0; axis < 3; ++axis) {
                if (direction[axis] == 0.0f) {
                    if (origin[axis] < bounds.Min[axis] || origin[axis] > bounds.Max[axis]) return false;
                    continue;
                }
                f32 first = (bounds.Min[axis] - origin[axis]) / direction[axis];
                f32 second = (bounds.Max[axis] - origin[axis]) / direction[axis];
                near = std::max(near, std::min(first, second));
                far = std::min(far, std::max(first, second));
            }
            return near <= far;
        }

        // Both faces of the triangle count.
        bool RayHitsTriangle(const Triangle& triangle, const Vec3& origin, const Vec3& direction, f32& outT) {
            Vec3 edge1 = triangle.B - triangle.A;
            Vec3 edge2 = triangle.C - triangle.A;
            Vec3 p = glm::cross(direction, edge2);
            f32 determinant = glm::dot(edge1, p);
            if (determinant == 0.0f) return false;

            Vec3 toOrigin = origin - triangle.A;
            f32 u = glm::dot(toOrigin, p) / determinant;
            if (u < 0.0f || u > 1.0f) return false;
            Vec3 q = glm::cross(toOrigin, edge1);
            f32 v = glm::dot(direction, q) / determinant;
            if (v < 0.0f || u + v > 1.0f) return false;
            outT = glm::dot(edge2, q) / determinant;
            return outT > 0.0f;
        }
    } // namespace

    namespace {
        constexpr u32 kMaxLeafTriangles = 4;
        // Each level adds at most one entry to the query stack, which holds 64.
        constexpr u32 kMaxDepth = 60;

        AABB GetTriangleBounds(const Triangle& triangle) {
            return AABB(glm::min(triangle.A, glm::min(triangle.B, triangle.C)),
                        glm::max(triangle.A, glm::max(triangle.B, triangle.C)));
        }
    } // namespace

    void TriangleTree::Build(const std::vector<Vec3>& positions, const std::vector<u32>& indices) {
        m_Nodes.clear();
        m_Triangles.clear();
        m_TriangleBounds.clear();

        const size_t vertexCount = positions.size();
        std::vector<Triangle> triangles;
        triangles.reserve(indices.size() / 3);
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            if (indices[i] >= vertexCount || indices[i + 1] >= vertexCount || indices[i + 2] >= vertexCount) continue;
            triangles.push_back({positions[indices[i]], positions[indices[i + 1]], positions[indices[i + 2]]});
        }
        if (triangles.empty()) return;

        m_Order.resize(triangles.size());
        m_Centroids.resize(triangles.size());
        for (u32 i = 0; i < triangles.size(); ++i) {
            m_Order[i] = i;
            m_Centroids[i] = (triangles[i].A + triangles[i].B + triangles[i].C) / 3.0f;
        }
        m_Triangles = std::move(triangles);

        m_Nodes.reserve(2 * m_Triangles.size() / kMaxLeafTriangles + 1);
        m_Nodes.emplace_back();
        BuildNode(0, 0, static_cast<u32>(m_Triangles.size()), 0);

        // Leaves refer to ranges of m_Order; store the triangles in that order instead.
        std::vector<Triangle> ordered;
        ordered.reserve(m_Triangles.size());
        for (u32 index : m_Order) {
            ordered.push_back(m_Triangles[index]);
        }
        m_Triangles = std::move(ordered);

        m_TriangleBounds.reserve(m_Triangles.size());
        for (const Triangle& triangle : m_Triangles) {
            m_TriangleBounds.push_back(GetTriangleBounds(triangle));
        }

        m_Order = {};
        m_Centroids = {};
    }

    bool TriangleTree::Raycast(const Vec3& origin, const Vec3& direction, f32 maxT, f32& outT, Vec3& outNormal) const {
        if (m_Nodes.empty()) return false;

        bool hit = false;
        f32 nearest = maxT;
        // Deep enough for the depth limit in BuildNode.
        u32 stack[64];
        u32 depth = 0;
        stack[depth++] = 0;
        while (depth > 0) {
            const Node& node = m_Nodes[stack[--depth]];
            if (!RayTouchesBounds(node.Bounds, origin, direction, nearest)) continue;
            if (node.Count == 0) {
                stack[depth++] = node.First;
                stack[depth++] = node.First + 1;
                continue;
            }
            for (u32 i = node.First; i < node.First + node.Count; ++i) {
                f32 t = 0.0f;
                if (RayHitsTriangle(m_Triangles[i], origin, direction, t) && t < nearest) {
                    nearest = t;
                    outNormal = glm::cross(m_Triangles[i].B - m_Triangles[i].A, m_Triangles[i].C - m_Triangles[i].A);
                    hit = true;
                }
            }
        }
        outT = nearest;
        return hit;
    }

    void TriangleTree::BuildNode(u32 nodeIndex, u32 first, u32 count, u32 depth) {
        AABB bounds = GetTriangleBounds(m_Triangles[m_Order[first]]);
        AABB centroidBounds(m_Centroids[m_Order[first]], m_Centroids[m_Order[first]]);
        for (u32 i = first + 1; i < first + count; ++i) {
            bounds.Merge(GetTriangleBounds(m_Triangles[m_Order[i]]));
            const Vec3& centroid = m_Centroids[m_Order[i]];
            centroidBounds.Min = glm::min(centroidBounds.Min, centroid);
            centroidBounds.Max = glm::max(centroidBounds.Max, centroid);
        }
        m_Nodes[nodeIndex].Bounds = bounds;

        Vec3 extent = centroidBounds.GetSize();
        int axis = extent.x >= extent.y && extent.x >= extent.z ? 0 : (extent.y >= extent.z ? 1 : 2);
        // Triangles whose centers all coincide can't be split.
        if (count <= kMaxLeafTriangles || depth >= kMaxDepth || extent[axis] <= 0.0f) {
            m_Nodes[nodeIndex].First = first;
            m_Nodes[nodeIndex].Count = count;
            return;
        }

        // Split at the median center along the longest axis.
        u32 half = count / 2;
        auto begin = m_Order.begin() + first;
        std::nth_element(begin, begin + half, begin + count,
                         [&](u32 a, u32 b) { return m_Centroids[a][axis] < m_Centroids[b][axis]; });

        u32 children = static_cast<u32>(m_Nodes.size());
        m_Nodes.emplace_back();
        m_Nodes.emplace_back();
        m_Nodes[nodeIndex].First = children;
        m_Nodes[nodeIndex].Count = 0;
        BuildNode(children, first, half, depth + 1);
        BuildNode(children + 1, first + half, count - half, depth + 1);
    }

} // namespace VKE::Physics
