#include "GizmoDrawing.h"
#include "TransformGizmo.h"
#include "Viewport/WorldShapes.h"

#include <ui/Style.h>

#include <algorithm>
#include <cmath>

namespace VKE::Editor {

    namespace Colors = UI::Colors;
    namespace Sizes = GizmoSizes;

    namespace {
        constexpr f32 kTwoPi = 6.28318530718f;
        constexpr u32 kAxisColors[3] = {Colors::AxisX, Colors::AxisY, Colors::AxisZ};

        // What drawing one gizmo needs to know.
        struct Painter {
            const TransformGizmo& Gizmo;
            const ScreenView& View;
            const GizmoLayout& Layout;
            Renderer::WorldDrawList& List;

            bool IsLit(GizmoHandle handle) const { return Gizmo.GetHovered() == handle; }

            // The handle's color: accent when hovered or dragged, faint while another one is dragged.
            u32 ColorOf(GizmoHandle handle, u32 rgba, f32 opacity) const {
                if (IsLit(handle)) return ToVertexColor(Colors::Accent, opacity);
                if (Gizmo.IsDragging()) opacity *= 0.35f;
                return ToVertexColor(rgba, opacity);
            }

            Vec3 Axis(u8 axis, f32 length) const {
                return Layout.Origin + Layout.Axes[axis] * (Layout.Length * length);
            }
        };

        void DrawCenter(const Painter& painter, u32 rgba, f32 opacity) {
            GizmoHandle handle{GizmoPart::Center, 0};
            f32 half = painter.View.GetPixelSize(painter.Layout.Origin) * Sizes::CenterPixels;
            Vec3 corners[4];
            GetFacingSquare(painter.Layout.Origin, painter.View.Right, painter.View.Up, half, corners);
            painter.List.AddQuad(corners, painter.ColorOf(handle, rgba, opacity * 0.35f));
            u32 outline = painter.ColorOf(handle, rgba, opacity);
            for (int i = 0; i < 4; ++i) {
                painter.List.AddLine(corners[i], corners[(i + 1) % 4], outline, 1.5f);
            }
        }

        void DrawMove(const Painter& painter) {
            const GizmoLayout& layout = painter.Layout;
            for (u8 axis = 0; axis < 3; ++axis) {
                f32 opacity = layout.PlaneOpacity[axis];
                if (opacity <= 0.01f) continue;
                GizmoHandle handle{GizmoPart::Plane, axis};
                Vec3 corners[4];
                layout.GetPlaneCorners(axis, corners);
                painter.List.AddQuad(corners, painter.ColorOf(handle, kAxisColors[axis], opacity * 0.3f));
                u32 outline = painter.ColorOf(handle, kAxisColors[axis], opacity);
                for (int i = 0; i < 4; ++i) {
                    painter.List.AddLine(corners[i], corners[(i + 1) % 4], outline, 1.5f);
                }
            }
            for (u8 axis = 0; axis < 3; ++axis) {
                f32 opacity = layout.AxisOpacity[axis];
                if (opacity <= 0.01f) continue;
                u32 color = painter.ColorOf({GizmoPart::Axis, axis}, kAxisColors[axis], opacity);
                painter.List.AddLine(painter.Axis(axis, Sizes::ArmStart), painter.Axis(axis, 1.0f), color,
                                     Sizes::LineWidth);
                AddCone(painter.List, painter.View.Position, painter.Axis(axis, 1.0f), layout.Axes[axis],
                        layout.Length * Sizes::HeadLength, layout.Length * Sizes::HeadRadius, color);
            }
            DrawCenter(painter, Colors::Text, 1.0f);
        }

        void DrawScale(const Painter& painter) {
            const GizmoLayout& layout = painter.Layout;
            for (u8 axis = 0; axis < 3; ++axis) {
                f32 opacity = layout.AxisOpacity[axis];
                if (opacity <= 0.01f) continue;
                u32 color = painter.ColorOf({GizmoPart::Axis, axis}, kAxisColors[axis], opacity);
                painter.List.AddLine(painter.Axis(axis, Sizes::ArmStart), painter.Axis(axis, 1.0f), color,
                                     Sizes::LineWidth);
                AddBox(painter.List, painter.View.Position, painter.Axis(axis, 1.0f), layout.Axes,
                       layout.Length * Sizes::ScaleBox, color);
            }
            GizmoHandle center{GizmoPart::Center, 0};
            AddBox(painter.List, painter.View.Position, layout.Origin, layout.Axes, layout.Length * Sizes::ScaleBox,
                   painter.ColorOf(center, Colors::Text, 1.0f));
        }

        void DrawRotate(const Painter& painter) {
            const GizmoLayout& layout = painter.Layout;
            const TransformGizmo& gizmo = painter.Gizmo;

            // The angle swept so far, as a fan on the ring it started on.
            if (gizmo.IsDragging() && std::abs(gizmo.GetDragAngle()) > 1e-4f) {
                const GizmoLayout& start = gizmo.GetDragLayout();
                GizmoHandle active = gizmo.GetActive();
                u32 fill = ToVertexColor(Colors::Accent, 0.18f);
                u32 steps = std::max(2u, static_cast<u32>(std::abs(gizmo.GetDragAngle()) / kTwoPi * 96.0f));
                auto ringPoint = [&](f32 angle) {
                    if (active.Part == GizmoPart::Axis) return start.GetRingPoint(active.Axis, angle);
                    f32 radius = start.Length * Sizes::ViewRingRadius;
                    return start.Origin +
                           (painter.View.Right * std::cos(angle) + painter.View.Up * std::sin(angle)) * radius;
                };
                Vec3 previous = ringPoint(gizmo.GetGrabAngle());
                for (u32 i = 1; i <= steps; ++i) {
                    Vec3 point = ringPoint(gizmo.GetGrabAngle() + gizmo.GetDragAngle() * i / steps);
                    painter.List.AddTriangle(start.Origin, previous, point, fill);
                    previous = point;
                }
                u32 edge = ToVertexColor(Colors::Accent, 0.8f);
                painter.List.AddLine(start.Origin, ringPoint(gizmo.GetGrabAngle()), edge, 1.5f);
                painter.List.AddLine(start.Origin, previous, edge, 1.5f);
            }

            // Back halves first and faint, so the front halves draw over them.
            for (bool front : {false, true}) {
                for (u8 axis = 0; axis < 3; ++axis) {
                    f32 opacity = front ? 1.0f : 0.2f;
                    u32 color = painter.ColorOf({GizmoPart::Axis, axis}, kAxisColors[axis], opacity);
                    Vec3 previous = layout.GetRingPoint(axis, 0.0f);
                    for (u32 i = 1; i <= Sizes::RingSegments; ++i) {
                        Vec3 point = layout.GetRingPoint(axis, kTwoPi * i / Sizes::RingSegments);
                        if (layout.IsFrontOfRing(axis, (previous + point) * 0.5f) == front) {
                            painter.List.AddLine(previous, point, color, front ? Sizes::LineWidth : 1.5f);
                        }
                        previous = point;
                    }
                }
            }

            u32 viewColor = painter.ColorOf({GizmoPart::ViewRing, 0}, Colors::Text, 0.8f);
            AddCircle(painter.List, layout.Origin, painter.View.Forward, layout.Length * Sizes::ViewRingRadius,
                      viewColor, 1.5f, Sizes::RingSegments);
        }
    } // namespace

    void DrawTransformGizmo(const TransformGizmo& gizmo, const ScreenView& view, const GizmoPose& pose,
                            Renderer::WorldDrawList& list) {
        GizmoLayout layout = GizmoLayout::Make(gizmo.Mode, gizmo.Space, view, pose);
        Renderer::WorldStyle style;
        style.Depth = Renderer::WorldDepth::OnTop;
        Renderer::WorldStyle previous = list.GetStyle();
        list.SetStyle(style);

        // Under the handles: a guide along the axis being moved or scaled, through where the drag started.
        if (gizmo.IsDragging() && gizmo.GetActive().Part == GizmoPart::Axis && gizmo.Mode != GizmoMode::Rotate) {
            const GizmoLayout& start = gizmo.GetDragLayout();
            Vec3 axis = start.Axes[gizmo.GetActive().Axis] * (start.Length * 40.0f);
            list.AddLine(start.Origin - axis, start.Origin + axis, ToVertexColor(Colors::Accent, 0.35f), 1.0f);
        }

        Painter painter{gizmo, view, layout, list};
        switch (gizmo.Mode) {
            case GizmoMode::Move: DrawMove(painter); break;
            case GizmoMode::Rotate: DrawRotate(painter); break;
            case GizmoMode::Scale: DrawScale(painter); break;
        }
        list.SetStyle(previous);
    }

} // namespace VKE::Editor
