#include "TransformGizmo.h"

#include <cmath>
#include <limits>

namespace VKE::Editor {

    namespace Sizes = GizmoSizes;

    namespace {
        constexpr f32 kTwoPi = 6.28318530718f;
        // Below this, an arrow or square is too edge-on to grab.
        constexpr f32 kMinOpacity = 0.1f;

        // The screen direction a world direction at `point` goes, or (1, 0) if it points at the camera.
        Vec2 ScreenDirection(const ScreenView& view, const Vec3& point, const Vec3& direction, f32 length) {
            Vec2 from;
            Vec2 to;
            if (!view.ToScreen(point, from) || !view.ToScreen(point + direction * length, to)) return Vec2(1.0f, 0.0f);
            Vec2 delta = to - from;
            f32 pixels = glm::length(delta);
            return pixels > 1e-3f ? delta / pixels : Vec2(1.0f, 0.0f);
        }

        // Keeps a scale away from 0, which can't be undone, on the side it started.
        f32 KeepNonZero(f32 value, f32 start) {
            constexpr f32 kMinScale = 0.001f;
            if (std::abs(value) >= kMinScale) return value;
            return start < 0.0f ? -kMinScale : kMinScale;
        }
    } // namespace

    bool TransformGizmo::Update(const ScreenView& view, const GizmoPointer& pointer, GizmoPose& pose) {
        if (IsDragging()) {
            if (pointer.Cancel) {
                pose = m_StartPose;
                m_Active = {};
                m_Hovered = {};
                return true;
            }
            if (pointer.Down) {
                GizmoPose before = pose;
                Drag(view, pointer, pose);
                m_Hovered = m_Active;
                return pose.Position != before.Position || pose.Rotation != before.Rotation ||
                       pose.Scale != before.Scale;
            }
            m_Active = {};
        }

        GizmoLayout layout = GizmoLayout::Make(Mode, Space, view, pose);
        m_Hovered = pointer.Over ? Pick(layout, view, pointer.Position) : GizmoHandle{};
        if (pointer.Pressed && m_Hovered.Part != GizmoPart::None) {
            if (BeginDrag(m_Hovered, view, pointer, pose)) m_Active = m_Hovered;
        }
        return false;
    }

    GizmoHandle TransformGizmo::Pick(const GizmoLayout& layout, const ScreenView& view, const Vec2& pixel) const {
        Vec2 origin;
        if (!view.ToScreen(layout.Origin, origin)) return {};

        GizmoHandle best;
        f32 bestDistance = Sizes::PickPixels;
        auto consider = [&](GizmoHandle handle, f32 distance) {
            if (distance < bestDistance) {
                bestDistance = distance;
                best = handle;
            }
        };

        if (Mode == GizmoMode::Rotate) {
            for (u8 axis = 0; axis < 3; ++axis) {
                // A ring seen edge on is a line that crosses the others; where they meet, the
                // ring seen more face on wins.
                f32 facing = std::abs(glm::dot(layout.Axes[axis], layout.ViewDirection));
                f32 penalty = (1.0f - glm::clamp(facing / 0.3f, 0.0f, 1.0f)) * 4.0f;
                Vec3 previous = layout.GetRingPoint(axis, 0.0f);
                for (u32 i = 1; i <= Sizes::RingSegments; ++i) {
                    Vec3 point = layout.GetRingPoint(axis, kTwoPi * i / Sizes::RingSegments);
                    Vec2 a;
                    Vec2 b;
                    if (layout.IsFrontOfRing(axis, (previous + point) * 0.5f) && view.ToScreen(previous, a) &&
                        view.ToScreen(point, b)) {
                        consider({GizmoPart::Axis, axis}, DistanceToSegment(pixel, a, b) + penalty);
                    }
                    previous = point;
                }
            }
            f32 ringPixels = Sizes::ScreenLength * Sizes::ViewRingRadius;
            consider({GizmoPart::ViewRing, 0}, std::abs(glm::length(pixel - origin) - ringPixels));
            return best;
        }

        // A center under the mouse wins over the arms around it.
        f32 centerPixels = Mode == GizmoMode::Scale ? Sizes::CenterPixels * 1.5f : Sizes::CenterPixels + 2.0f;
        if (glm::length(pixel - origin) <= centerPixels) return {GizmoPart::Center, 0};

        if (Mode == GizmoMode::Move) {
            for (u8 axis = 0; axis < 3; ++axis) {
                if (layout.PlaneOpacity[axis] < kMinOpacity) continue;
                Vec3 corners[4];
                layout.GetPlaneCorners(axis, corners);
                Vec2 screen[4];
                bool visible = true;
                for (int i = 0; i < 4; ++i) {
                    visible = visible && view.ToScreen(corners[i], screen[i]);
                }
                if (visible && IsInsideQuad(pixel, screen)) return {GizmoPart::Plane, axis};
            }
        }

        f32 end = Mode == GizmoMode::Move ? 1.0f + Sizes::HeadLength : 1.0f + Sizes::ScaleBox;
        for (u8 axis = 0; axis < 3; ++axis) {
            if (layout.AxisOpacity[axis] < kMinOpacity) continue;
            Vec2 a;
            Vec2 b;
            Vec3 direction = layout.Axes[axis] * layout.Length;
            if (view.ToScreen(layout.Origin + direction * Sizes::ArmStart, a) &&
                view.ToScreen(layout.Origin + direction * end, b)) {
                consider({GizmoPart::Axis, axis}, DistanceToSegment(pixel, a, b));
            }
        }
        return best;
    }

    bool TransformGizmo::BeginDrag(GizmoHandle handle, const ScreenView& view, const GizmoPointer& pointer,
                                   const GizmoPose& pose) {
        m_Start = GizmoLayout::Make(Mode, Space, view, pose);
        m_StartPose = pose;
        m_StartPixel = pointer.Position;
        m_Angle = 0.0f;
        Ray ray = view.GetRay(pointer.Position);

        if (Mode == GizmoMode::Move) {
            if (handle.Part == GizmoPart::Axis) {
                m_StartAlong = ClosestOnLine(ray, m_Start.Origin, m_Start.Axes[handle.Axis]);
                return true;
            }
            m_PlaneNormal = handle.Part == GizmoPart::Plane ? m_Start.Axes[handle.Axis] : view.Forward;
            f32 distance = 0.0f;
            if (!IntersectPlane(ray, m_Start.Origin, m_PlaneNormal, distance)) return false;
            m_StartHit = ray.GetPoint(distance);
            return true;
        }

        if (Mode == GizmoMode::Rotate) {
            // Turning follows the mouse along the ring's tangent where it was grabbed.
            Vec3 grab;
            if (handle.Part == GizmoPart::ViewRing) {
                m_RotationAxis = -view.Forward;
                Vec2 origin;
                view.ToScreen(m_Start.Origin, origin);
                Vec2 offset = pointer.Position - origin;
                m_GrabAngle = std::atan2(-offset.y, offset.x);
                f32 radius = m_Start.Length * Sizes::ViewRingRadius;
                grab = m_Start.Origin +
                       (view.Right * std::cos(m_GrabAngle) + view.Up * std::sin(m_GrabAngle)) * radius;
                m_PixelsPerUnit = Sizes::ScreenLength * Sizes::ViewRingRadius;
            } else {
                m_RotationAxis = m_Start.Axes[handle.Axis];
                f32 bestDistance = std::numeric_limits<f32>::max();
                for (u32 i = 0; i < Sizes::RingSegments; ++i) {
                    f32 angle = kTwoPi * i / Sizes::RingSegments;
                    Vec2 point;
                    if (!view.ToScreen(m_Start.GetRingPoint(handle.Axis, angle), point)) continue;
                    f32 distance = glm::length(point - pointer.Position);
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        m_GrabAngle = angle;
                    }
                }
                grab = m_Start.GetRingPoint(handle.Axis, m_GrabAngle);
                m_PixelsPerUnit = Sizes::ScreenLength * Sizes::RingRadius;
            }
            Vec3 tangent = glm::normalize(glm::cross(m_RotationAxis, grab - m_Start.Origin));
            m_ScreenDirection = ScreenDirection(view, grab, tangent, m_Start.Length * 0.1f);
            return true;
        }

        // Scale: growing follows the mouse along the arm, or up and right for all axes.
        if (handle.Part == GizmoPart::Axis) {
            m_ScreenDirection = ScreenDirection(view, m_Start.Origin, m_Start.Axes[handle.Axis], m_Start.Length);
            Vec2 from;
            Vec2 to;
            m_PixelsPerUnit = Sizes::ScreenLength;
            if (view.ToScreen(m_Start.Origin, from) &&
                view.ToScreen(m_Start.Origin + m_Start.Axes[handle.Axis] * m_Start.Length, to)) {
                m_PixelsPerUnit = std::max(glm::length(to - from), 10.0f);
            }
        } else {
            m_ScreenDirection = glm::normalize(Vec2(1.0f, -1.0f));
            m_PixelsPerUnit = Sizes::ScreenLength;
        }
        return true;
    }

    void TransformGizmo::Drag(const ScreenView& view, const GizmoPointer& pointer, GizmoPose& pose) {
        Ray ray = view.GetRay(pointer.Position);
        Vec2 moved = pointer.Position - m_StartPixel;

        if (Mode == GizmoMode::Move) {
            if (m_Active.Part == GizmoPart::Axis) {
                const Vec3& axis = m_Start.Axes[m_Active.Axis];
                f32 delta = ClosestOnLine(ray, m_Start.Origin, axis) - m_StartAlong;
                if (pointer.Snap) delta = Snap(delta, MoveStep);
                pose.Position = m_StartPose.Position + axis * delta;
                return;
            }

            f32 distance = 0.0f;
            if (!IntersectPlane(ray, m_Start.Origin, m_PlaneNormal, distance)) return;
            Vec3 delta = ray.GetPoint(distance) - m_StartHit;
            if (pointer.Snap) {
                if (m_Active.Part == GizmoPart::Plane) {
                    const Vec3& a = m_Start.Axes[(m_Active.Axis + 1) % 3];
                    const Vec3& b = m_Start.Axes[(m_Active.Axis + 2) % 3];
                    delta = a * Snap(glm::dot(delta, a), MoveStep) + b * Snap(glm::dot(delta, b), MoveStep);
                } else {
                    delta = Vec3(Snap(delta.x, MoveStep), Snap(delta.y, MoveStep), Snap(delta.z, MoveStep));
                }
            }
            pose.Position = m_StartPose.Position + delta;
            return;
        }

        f32 along = glm::dot(moved, m_ScreenDirection) / m_PixelsPerUnit;

        if (Mode == GizmoMode::Rotate) {
            m_Angle = pointer.Snap ? Snap(along, RotateStep) : along;
            pose.Rotation = glm::normalize(glm::angleAxis(m_Angle, m_RotationAxis) * m_StartPose.Rotation);
            return;
        }

        f32 factor = 1.0f + along;
        if (m_Active.Part == GizmoPart::Axis) {
            u8 axis = m_Active.Axis;
            f32 value = m_StartPose.Scale[axis] * factor;
            if (pointer.Snap) value = Snap(value, ScaleStep);
            pose.Scale = m_StartPose.Scale;
            pose.Scale[axis] = KeepNonZero(value, m_StartPose.Scale[axis]);
            return;
        }
        if (pointer.Snap) factor = Snap(factor, ScaleStep);
        for (int axis = 0; axis < 3; ++axis) {
            pose.Scale[axis] = KeepNonZero(m_StartPose.Scale[axis] * factor, m_StartPose.Scale[axis]);
        }
    }

} // namespace VKE::Editor
