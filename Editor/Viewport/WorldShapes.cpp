#include "WorldShapes.h"
#include "Gizmo/GizmoMath.h"

#include <algorithm>
#include <cmath>

namespace VKE::Editor {

    namespace {
        constexpr f32 kTwoPi = 6.28318530718f;

        // The same color with its red, green and blue scaled; alpha kept.
        u32 Shade(u32 color, f32 factor) {
            u32 result = color & 0xFF000000u;
            for (int shift = 0; shift < 24; shift += 8) {
                u32 channel = static_cast<u32>(static_cast<f32>((color >> shift) & 0xFFu) * factor);
                result |= std::min(channel, 255u) << shift;
            }
            return result;
        }

        bool FacesView(const Vec3& viewPosition, const Vec3& a, const Vec3& b, const Vec3& c) {
            return glm::dot(glm::cross(b - a, c - a), viewPosition - a) > 0.0f;
        }

        void AddFacingTriangle(Renderer::WorldDrawList& list, const Vec3& viewPosition, const Vec3& a, const Vec3& b,
                               const Vec3& c, u32 color) {
            if (FacesView(viewPosition, a, b, c)) list.AddTriangle(a, b, c, color);
        }
    } // namespace

    u32 ToVertexColor(u32 rgba, f32 opacity) {
        u32 alpha = static_cast<u32>(static_cast<f32>(rgba & 0xFFu) * glm::clamp(opacity, 0.0f, 1.0f) + 0.5f);
        // R, G, B, A bytes in memory order.
        return ((rgba >> 24) & 0xFFu) | (((rgba >> 16) & 0xFFu) << 8) | (((rgba >> 8) & 0xFFu) << 16) |
               (alpha << 24);
    }

    void AddCircle(Renderer::WorldDrawList& list, const Vec3& center, const Vec3& normal, f32 radius, u32 color,
                   f32 width, u32 segments) {
        Vec3 u;
        Vec3 v;
        MakeBasis(normal, u, v);
        Vec3 previous = center + u * radius;
        for (u32 i = 1; i <= segments; ++i) {
            f32 angle = kTwoPi * static_cast<f32>(i) / static_cast<f32>(segments);
            Vec3 point = center + (u * std::cos(angle) + v * std::sin(angle)) * radius;
            list.AddLine(previous, point, color, width);
            previous = point;
        }
    }

    void AddCone(Renderer::WorldDrawList& list, const Vec3& viewPosition, const Vec3& base, const Vec3& direction,
                 f32 length, f32 radius, u32 color, u32 segments) {
        Vec3 u;
        Vec3 v;
        MakeBasis(direction, u, v);
        Vec3 tip = base + direction * length;
        u32 dark = Shade(color, 0.7f);
        Vec3 previous = base + u * radius;
        for (u32 i = 1; i <= segments; ++i) {
            f32 angle = kTwoPi * static_cast<f32>(i) / static_cast<f32>(segments);
            Vec3 point = base + (u * std::cos(angle) + v * std::sin(angle)) * radius;
            AddFacingTriangle(list, viewPosition, previous, point, tip, color);
            AddFacingTriangle(list, viewPosition, point, previous, base, dark);
            previous = point;
        }
    }

    void AddBox(Renderer::WorldDrawList& list, const Vec3& viewPosition, const Vec3& center, const Vec3 axes[3],
                f32 halfSize, u32 color) {
        // Lit from above and a little in front, by a fixed light.
        const Vec3 light = glm::normalize(Vec3(0.3f, 1.0f, 0.5f));
        for (int axis = 0; axis < 3; ++axis) {
            const Vec3& u = axes[(axis + 1) % 3];
            const Vec3& v = axes[(axis + 2) % 3];
            for (f32 side : {-1.0f, 1.0f}) {
                Vec3 normal = axes[axis] * side;
                Vec3 face = center + normal * halfSize;
                if (glm::dot(normal, viewPosition - face) <= 0.0f) continue;
                Vec3 corners[4] = {face - u * halfSize - v * halfSize, face + u * halfSize - v * halfSize,
                                   face + u * halfSize + v * halfSize, face - u * halfSize + v * halfSize};
                list.AddQuad(corners, Shade(color, 0.65f + 0.35f * std::max(glm::dot(normal, light), 0.0f)));
            }
        }
    }

    void GetFacingSquare(const Vec3& center, const Vec3& right, const Vec3& up, f32 halfSize, Vec3 corners[4]) {
        // Top left first, so a texture shows upright.
        corners[0] = center - right * halfSize + up * halfSize;
        corners[1] = center + right * halfSize + up * halfSize;
        corners[2] = center + right * halfSize - up * halfSize;
        corners[3] = center - right * halfSize - up * halfSize;
    }

} // namespace VKE::Editor
