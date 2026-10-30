#pragma once

#include "GizmoMath.h"

#include <core/Types.h>
#include <math/MathTypes.h>

// Where the transform gizmo's handles are, shared by the part that picks them with the mouse
// and the part that draws them, so both always agree.

namespace VKE::Editor {

    enum class GizmoMode : u8 {
        Move,
        Rotate,
        Scale,
    };

    // The axes moving and rotating follow. Scaling always uses the object's own axes.
    enum class GizmoSpace : u8 {
        World,
        Local,
    };

    // A position, a rotation and a scale, as the gizmo edits them: the position and rotation in
    // the world, the scale relative to the parent.
    struct GizmoPose {
        Vec3 Position{0.0f};
        Quat Rotation{1.0f, 0.0f, 0.0f, 0.0f};
        Vec3 Scale{1.0f};
    };

    enum class GizmoPart : u8 {
        None,
        // Move and scale arrows, rotation rings.
        Axis,
        // Move: the square between the two axes other than Axis.
        Plane,
        // Move: in the view's plane. Scale: all axes at once.
        Center,
        // Rotate: the outer ring, around the view's direction.
        ViewRing,
    };

    struct GizmoHandle {
        GizmoPart Part = GizmoPart::None;
        // 0, 1, 2 for X, Y, Z, for Axis and Plane.
        u8 Axis = 0;

        bool operator==(const GizmoHandle& other) const { return Part == other.Part && Axis == other.Axis; }
        bool operator!=(const GizmoHandle& other) const { return !(*this == other); }
    };

    // Sizes in units of the gizmo's length, except where they say pixels.
    namespace GizmoSizes {
        // How long the arms are on screen, whatever the distance.
        constexpr f32 ScreenLength = 110.0f;
        constexpr f32 ArmStart = 0.15f;
        constexpr f32 HeadLength = 0.22f;
        constexpr f32 HeadRadius = 0.065f;
        constexpr f32 PlaneMin = 0.25f;
        constexpr f32 PlaneMax = 0.45f;
        constexpr f32 CenterPixels = 7.0f;
        constexpr f32 RingRadius = 0.95f;
        constexpr f32 ViewRingRadius = 1.12f;
        constexpr f32 ScaleBox = 0.06f;
        constexpr u32 RingSegments = 64;
        // How near the mouse must be to a handle.
        constexpr f32 PickPixels = 7.0f;
        constexpr f32 LineWidth = 2.5f;
    } // namespace GizmoSizes

    struct GizmoLayout {
        Vec3 Origin{0.0f};
        Vec3 Axes[3] = {Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)};
        // The world length the gizmo's length is drawn at.
        f32 Length = 1.0f;
        // 1 or -1: the side of each axis facing the camera, where the plane squares go.
        f32 Signs[3] = {1.0f, 1.0f, 1.0f};
        // Arrows and squares fade out as they turn edge-on to the view, where they can't be
        // dragged well; 0 is hidden.
        f32 AxisOpacity[3] = {1.0f, 1.0f, 1.0f};
        f32 PlaneOpacity[3] = {1.0f, 1.0f, 1.0f};
        // From the camera to the origin, unit length.
        Vec3 ViewDirection{0.0f, 0.0f, -1.0f};

        static GizmoLayout Make(GizmoMode mode, GizmoSpace space, const ScreenView& view, const GizmoPose& pose);

        // The four corners of the plane square whose normal is axis `axis`.
        void GetPlaneCorners(u8 axis, Vec3 corners[4]) const;
        // The point of ring `axis` at `angle` radians from its start.
        Vec3 GetRingPoint(u8 axis, f32 angle) const;
        // Whether a point of ring `axis` faces the camera; the back halves are only drawn
        // faintly. A ring seen almost face on is all in front.
        bool IsFrontOfRing(u8 axis, const Vec3& point) const;
    };

} // namespace VKE::Editor
