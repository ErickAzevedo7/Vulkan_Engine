#pragma once

#include "GizmoLayout.h"
#include "GizmoMath.h"

#include <core/Types.h>
#include <math/MathTypes.h>

namespace VKE::Editor {

    // The mouse over a viewport, as the gizmo needs it. Pixels count from the viewport's top left.
    struct GizmoPointer {
        Vec2 Position{0.0f};
        // The mouse is over the viewport and nothing else wants it.
        bool Over = false;
        // The button went down this frame.
        bool Pressed = false;
        bool Down = false;
        // Rounds the change to the steps while held.
        bool Snap = false;
        // Ends a drag and puts the pose back.
        bool Cancel = false;
    };

    // Moves, rotates and scales a pose with handles picked by the mouse: arrows, squares and a
    // center to move, rings to rotate, arrows with boxes and a center to scale. Drags work from
    // where they started, so snapping and cancelling are exact. Drawing is GizmoDrawing's.
    class TransformGizmo {
    public:
        GizmoMode Mode = GizmoMode::Move;
        GizmoSpace Space = GizmoSpace::World;
        // Snapping steps: meters, radians and scale units.
        f32 MoveStep = 0.5f;
        f32 RotateStep = glm::radians(15.0f);
        f32 ScaleStep = 0.1f;

        // Hovers the handle under the pointer, and on a press over one starts dragging it.
        // While dragging, changes `pose`; true when it changed.
        bool Update(const ScreenView& view, const GizmoPointer& pointer, GizmoPose& pose);

        GizmoHandle GetHovered() const { return m_Hovered; }
        GizmoHandle GetActive() const { return m_Active; }
        bool IsDragging() const { return m_Active.Part != GizmoPart::None; }

        // While rotating: the handles as they were when the drag started, the angle on the
        // ring that was grabbed, and how far it has turned since, in radians.
        const GizmoLayout& GetDragLayout() const { return m_Start; }
        f32 GetGrabAngle() const { return m_GrabAngle; }
        f32 GetDragAngle() const { return m_Angle; }

    private:
        GizmoHandle Pick(const GizmoLayout& layout, const ScreenView& view, const Vec2& pixel) const;
        bool BeginDrag(GizmoHandle handle, const ScreenView& view, const GizmoPointer& pointer, const GizmoPose& pose);
        void Drag(const ScreenView& view, const GizmoPointer& pointer, GizmoPose& pose);

        GizmoHandle m_Hovered;
        GizmoHandle m_Active;

        // Where the drag started.
        GizmoLayout m_Start;
        GizmoPose m_StartPose;
        Vec2 m_StartPixel{0.0f};
        // Moving along an axis: the distance along it under the mouse.
        f32 m_StartAlong = 0.0f;
        // Moving in a plane: the point of the plane under the mouse, and the plane.
        Vec3 m_StartHit{0.0f};
        Vec3 m_PlaneNormal{0.0f, 1.0f, 0.0f};
        // Rotating and scaling: the screen direction that turns or grows, and how many pixels
        // along it make one radian or one scale unit.
        Vec2 m_ScreenDirection{1.0f, 0.0f};
        f32 m_PixelsPerUnit = 1.0f;
        Vec3 m_RotationAxis{0.0f, 1.0f, 0.0f};
        f32 m_GrabAngle = 0.0f;
        f32 m_Angle = 0.0f;
    };

} // namespace VKE::Editor
