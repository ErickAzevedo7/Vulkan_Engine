#pragma once

#include <core/Types.h>
#include <math/AABB.h>
#include <math/MathTypes.h>

#include <vector>

namespace VKE::Physics {

    struct Triangle {
        Vec3 A;
        Vec3 B;
        Vec3 C;
    };

    // Bounding volume tree over a static triangle mesh, so a query only visits the triangles
    // near the region it asks about instead of all of them.
    class TriangleTree {
    public:
        // `indices` holds three entries per triangle; incomplete or out of range ones are skipped.
        void Build(const std::vector<Vec3>& positions, const std::vector<u32>& indices);

        bool IsEmpty() const { return m_Nodes.empty(); }
        // The whole mesh, in its own space.
        const AABB& GetBounds() const { return m_Nodes.front().Bounds; }
        size_t GetTriangleCount() const { return m_Triangles.size(); }

        // Calls `visit(const Triangle&)` for each triangle whose bounds touch `bounds`.
        template<typename Visitor>
        void Query(const AABB& bounds, Visitor&& visit) const {
            if (m_Nodes.empty()) return;

            // Deep enough for the depth limit in BuildNode.
            u32 stack[64];
            u32 depth = 0;
            stack[depth++] = 0;
            while (depth > 0) {
                const Node& node = m_Nodes[stack[--depth]];
                if (!node.Bounds.Intersects(bounds)) continue;

                if (node.Count > 0) {
                    for (u32 i = node.First; i < node.First + node.Count; ++i) {
                        if (m_TriangleBounds[i].Intersects(bounds)) visit(m_Triangles[i]);
                    }
                } else {
                    // Children are stored side by side.
                    stack[depth++] = node.First;
                    stack[depth++] = node.First + 1;
                }
            }
        }

    private:
        struct Node {
            AABB Bounds;
            // A leaf's first triangle, or an inner node's first child.
            u32 First = 0;
            // Triangles in a leaf; 0 for an inner node.
            u32 Count = 0;
        };

        void BuildNode(u32 nodeIndex, u32 first, u32 count, u32 depth);

        std::vector<Node> m_Nodes;
        std::vector<Triangle> m_Triangles;
        std::vector<AABB> m_TriangleBounds;
        // Used only while building: triangles in tree order, and their centers.
        std::vector<u32> m_Order;
        std::vector<Vec3> m_Centroids;
    };

} // namespace VKE::Physics
