#include "GizmoMath.h"

#include <cmath>

namespace VKE::Editor {

    ScreenView ScreenView::From(const Renderer::RenderView& view, const Vec2& size) {
        ScreenView screen;
        screen.Size = glm::max(size, Vec2(1.0f));
        screen.ViewProjection = view.GetProjection(screen.Size.x / screen.Size.y) * view.View;
        screen.InverseViewProjection = glm::inverse(screen.ViewProjection);

        Mat4 camera = glm::inverse(view.View);
        screen.Position = Vec3(camera[3]);
        screen.Right = glm::normalize(Vec3(camera[0]));
        screen.Up = glm::normalize(Vec3(camera[1]));
        screen.Forward = -glm::normalize(Vec3(camera[2]));
        screen.TanHalfFov = std::tan(view.FieldOfView * 0.5f);
        return screen;
    }

    bool ScreenView::ToScreen(const Vec3& point, Vec2& pixel) const {
        Vec4 clip = ViewProjection * Vec4(point, 1.0f);
        if (clip.w <= 1e-5f) return false;
        Vec2 ndc = Vec2(clip) / clip.w;
        // Clip space is Y-up; pixels go down.
        pixel = Vec2((ndc.x * 0.5f + 0.5f) * Size.x, (0.5f - ndc.y * 0.5f) * Size.y);
        return true;
    }

    Ray ScreenView::GetRay(const Vec2& pixel) const {
        Vec2 ndc(pixel.x / Size.x * 2.0f - 1.0f, 1.0f - pixel.y / Size.y * 2.0f);
        Vec4 far = InverseViewProjection * Vec4(ndc, 1.0f, 1.0f);
        Ray ray;
        ray.Origin = Position;
        ray.Direction = glm::normalize(Vec3(far) / far.w - Position);
        return ray;
    }

    f32 ScreenView::GetPixelSize(const Vec3& point) const {
        f32 depth = std::max(glm::dot(point - Position, Forward), 1e-4f);
        return depth * 2.0f * TanHalfFov / Size.y;
    }

    bool IntersectPlane(const Ray& ray, const Vec3& planePoint, const Vec3& planeNormal, f32& distance) {
        f32 facing = glm::dot(ray.Direction, planeNormal);
        if (std::abs(facing) < 1e-6f) return false;
        distance = glm::dot(planePoint - ray.Origin, planeNormal) / facing;
        return distance >= 0.0f;
    }

    f32 ClosestOnLine(const Ray& ray, const Vec3& linePoint, const Vec3& lineDirection) {
        // Closest points of two lines, with both directions at unit length.
        Vec3 between = linePoint - ray.Origin;
        f32 along = glm::dot(lineDirection, ray.Direction);
        f32 denominator = 1.0f - along * along;
        if (denominator < 1e-6f) return 0.0f;
        return (along * glm::dot(ray.Direction, between) - glm::dot(lineDirection, between)) / denominator;
    }

    f32 DistanceToSegment(const Vec2& point, const Vec2& a, const Vec2& b) {
        Vec2 segment = b - a;
        f32 lengthSquared = glm::dot(segment, segment);
        f32 t = lengthSquared > 0.0f ? glm::clamp(glm::dot(point - a, segment) / lengthSquared, 0.0f, 1.0f) : 0.0f;
        return glm::length(point - (a + segment * t));
    }

    bool IsInsideQuad(const Vec2& point, const Vec2 corners[4]) {
        // Inside when the point is on the same side of every edge.
        f32 sign = 0.0f;
        for (int i = 0; i < 4; ++i) {
            Vec2 edge = corners[(i + 1) % 4] - corners[i];
            Vec2 toPoint = point - corners[i];
            f32 cross = edge.x * toPoint.y - edge.y * toPoint.x;
            if (cross == 0.0f) continue;
            if (sign == 0.0f) {
                sign = cross;
            } else if ((cross > 0.0f) != (sign > 0.0f)) {
                return false;
            }
        }
        return true;
    }

    void MakeBasis(const Vec3& normal, Vec3& u, Vec3& v) {
        Vec3 other = std::abs(normal.y) < 0.99f ? Vec3(0.0f, 1.0f, 0.0f) : Vec3(1.0f, 0.0f, 0.0f);
        u = glm::normalize(glm::cross(other, normal));
        v = glm::cross(normal, u);
    }

    f32 Snap(f32 value, f32 step) {
        return step > 0.0f ? std::round(value / step) * step : value;
    }

} // namespace VKE::Editor
