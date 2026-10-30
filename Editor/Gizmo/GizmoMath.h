#pragma once

#include <core/Types.h>
#include <math/MathTypes.h>
#include <renderer/RenderTypes.h>

// Geometry for working with the scene through the screen: rays from the mouse, points
// projected to pixels, planes and lines. Knows nothing about handles or the UI.

namespace VKE::Editor {

    struct Ray {
        Vec3 Origin{0.0f};
        // Unit length.
        Vec3 Direction{0.0f, 0.0f, -1.0f};

        Vec3 GetPoint(f32 distance) const { return Origin + Direction * distance; }
    };

    // A view of the scene as drawn into a viewport: projects points to its pixels and pixels
    // back to rays. Pixels count from the viewport's top left.
    struct ScreenView {
        Mat4 ViewProjection{1.0f};
        Mat4 InverseViewProjection{1.0f};
        Vec3 Position{0.0f};
        Vec3 Forward{0.0f, 0.0f, -1.0f};
        Vec3 Right{1.0f, 0.0f, 0.0f};
        Vec3 Up{0.0f, 1.0f, 0.0f};
        Vec2 Size{1.0f};
        // tan(vertical field of view / 2).
        f32 TanHalfFov = 0.5f;

        static ScreenView From(const Renderer::RenderView& view, const Vec2& size);

        // False for points behind the camera.
        bool ToScreen(const Vec3& point, Vec2& pixel) const;
        // From the camera through the middle of `pixel`.
        Ray GetRay(const Vec2& pixel) const;
        // How long one pixel is in the world, at the depth of `point`.
        f32 GetPixelSize(const Vec3& point) const;
    };

    // Where the ray crosses the plane, as a distance along the ray. False when it runs along
    // the plane or crosses it behind its origin.
    bool IntersectPlane(const Ray& ray, const Vec3& planePoint, const Vec3& planeNormal, f32& distance);

    // The point of the line closest to the ray, as a distance from `linePoint` along
    // `lineDirection` (unit length). 0 when they run parallel.
    f32 ClosestOnLine(const Ray& ray, const Vec3& linePoint, const Vec3& lineDirection);

    // Pixels from `point` to the segment from `a` to `b`.
    f32 DistanceToSegment(const Vec2& point, const Vec2& a, const Vec2& b);

    // `corners` go around a convex quad, in either direction.
    bool IsInsideQuad(const Vec2& point, const Vec2 corners[4]);

    // Two unit vectors at right angles to `normal` and to each other.
    void MakeBasis(const Vec3& normal, Vec3& u, Vec3& v);

    // Rounds `value` to a multiple of `step`; a step of 0 leaves it.
    f32 Snap(f32 value, f32 step);

} // namespace VKE::Editor
