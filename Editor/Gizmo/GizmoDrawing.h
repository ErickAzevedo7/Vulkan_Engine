#pragma once

#include "GizmoLayout.h"
#include "GizmoMath.h"

#include <renderer/WorldDraw.h>

namespace VKE::Editor {

    class TransformGizmo;

    // Draws the gizmo's handles for `pose` over everything, in the axis colors, with the
    // hovered or dragged handle in the accent color.
    void DrawTransformGizmo(const TransformGizmo& gizmo, const ScreenView& view, const GizmoPose& pose,
                            Renderer::WorldDrawList& list);

} // namespace VKE::Editor
