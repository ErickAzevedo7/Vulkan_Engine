#pragma once

#include <core/Types.h>
#include <math/MathTypes.h>
#include <renderer/WorldDraw.h>

// Shapes the editor draws in the world, built from the renderer's lines and triangles.

namespace VKE::Editor {

    // A 0xRRGGBBAA design system color as the renderer's vertices take it, with its alpha
    // scaled by `opacity`.
    u32 ToVertexColor(u32 rgba, f32 opacity = 1.0f);

    // Around `normal`, in `segments` straight lines.
    void AddCircle(Renderer::WorldDrawList& list, const Vec3& center, const Vec3& normal, f32 radius, u32 color,
                   f32 width, u32 segments = 48);

    // Solid shapes only get the faces that look at `viewPosition`, so they read right even
    // drawn over everything, without depth.

    // Pointing from `base` along `direction` (unit length).
    void AddCone(Renderer::WorldDrawList& list, const Vec3& viewPosition, const Vec3& base, const Vec3& direction,
                 f32 length, f32 radius, u32 color, u32 segments = 16);

    // Around `center`, with sides along `axes` (unit length). Faces turned from the light
    // come out a little darker, so the box reads as solid.
    void AddBox(Renderer::WorldDrawList& list, const Vec3& viewPosition, const Vec3& center, const Vec3 axes[3],
                f32 halfSize, u32 color);

    // The four corners of a square facing the camera, `halfSize` world units from its center.
    void GetFacingSquare(const Vec3& center, const Vec3& right, const Vec3& up, f32 halfSize, Vec3 corners[4]);

} // namespace VKE::Editor
