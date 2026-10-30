#include "GizmoLayout.h"

#include <cmath>

namespace VKE::Editor {

    namespace {
        f32 SmoothStep(f32 edge0, f32 edge1, f32 x) {
            f32 t = glm::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }
    } // namespace

    GizmoLayout GizmoLayout::Make(GizmoMode mode, GizmoSpace space, const ScreenView& view, const GizmoPose& pose) {
        GizmoLayout layout;
        layout.Origin = pose.Position;
        layout.Length = view.GetPixelSize(pose.Position) * GizmoSizes::ScreenLength;

        if (space == GizmoSpace::Local || mode == GizmoMode::Scale) {
            Mat3 rotation = glm::mat3_cast(glm::normalize(pose.Rotation));
            for (int i = 0; i < 3; ++i) {
                layout.Axes[i] = glm::normalize(rotation[i]);
            }
        }

        Vec3 toOrigin = pose.Position - view.Position;
        f32 distance = glm::length(toOrigin);
        layout.ViewDirection = distance > 1e-5f ? toOrigin / distance : view.Forward;

        for (int i = 0; i < 3; ++i) {
            f32 facing = glm::dot(layout.Axes[i], layout.ViewDirection);
            f32 along = std::abs(facing);
            // Only an axis clearly pointing away flips; one seen from the side keeps its positive side.
            layout.Signs[i] = facing > 0.3f ? -1.0f : 1.0f;
            layout.AxisOpacity[i] = 1.0f - SmoothStep(0.94f, 0.99f, along);
            layout.PlaneOpacity[i] = SmoothStep(0.1f, 0.25f, along);
        }
        return layout;
    }

    void GizmoLayout::GetPlaneCorners(u8 axis, Vec3 corners[4]) const {
        u8 a = (axis + 1) % 3;
        u8 b = (axis + 2) % 3;
        Vec3 u = Axes[a] * (Signs[a] * Length);
        Vec3 v = Axes[b] * (Signs[b] * Length);
        corners[0] = Origin + u * GizmoSizes::PlaneMin + v * GizmoSizes::PlaneMin;
        corners[1] = Origin + u * GizmoSizes::PlaneMax + v * GizmoSizes::PlaneMin;
        corners[2] = Origin + u * GizmoSizes::PlaneMax + v * GizmoSizes::PlaneMax;
        corners[3] = Origin + u * GizmoSizes::PlaneMin + v * GizmoSizes::PlaneMax;
    }

    Vec3 GizmoLayout::GetRingPoint(u8 axis, f32 angle) const {
        // Counterclockwise around the axis, like a positive rotation.
        const Vec3& u = Axes[(axis + 1) % 3];
        const Vec3& v = Axes[(axis + 2) % 3];
        return Origin + (u * std::cos(angle) + v * std::sin(angle)) * (Length * GizmoSizes::RingRadius);
    }

    bool GizmoLayout::IsFrontOfRing(u8 axis, const Vec3& point) const {
        if (std::abs(glm::dot(Axes[axis], ViewDirection)) > 0.85f) return true;
        return glm::dot(point - Origin, ViewDirection) <= Length * 0.02f;
    }

} // namespace VKE::Editor
