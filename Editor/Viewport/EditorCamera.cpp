#include "EditorCamera.h"

#include <algorithm>
#include <cmath>

namespace VKE::Editor {

    namespace {
        // Radians per pixel of mouse motion.
        constexpr f32 kTurnSpeed = 0.004f;
        // Just short of straight up or down, where yaw stops meaning anything.
        constexpr f32 kMaxPitch = 1.55f;
        constexpr f32 kMinDistance = 0.05f;
        constexpr f32 kMinFlySpeed = 0.1f;
        constexpr f32 kMaxFlySpeed = 500.0f;
    } // namespace

    EditorCamera::EditorCamera() {
        m_Camera.FarPlane = 2000.0f;
        LookAt(Vec3(6.0f, 4.0f, 8.0f), Vec3(0.0f));
    }

    void EditorCamera::LookAt(const Vec3& position, const Vec3& target) {
        m_Camera.Position = position;
        m_Camera.LookAt(target);
        m_Distance = std::max(glm::length(target - position), kMinDistance);
    }

    void EditorCamera::Fly(const Vec2& pixels, const Vec3& move, f32 deltaTime, bool fast) {
        m_Camera.Yaw += pixels.x * kTurnSpeed;
        m_Camera.Pitch = glm::clamp(m_Camera.Pitch - pixels.y * kTurnSpeed, -kMaxPitch, kMaxPitch);

        f32 speed = m_FlySpeed * (fast ? 3.0f : 1.0f) * deltaTime;
        Vec3 up(0.0f, 1.0f, 0.0f);
        m_Camera.Position += (m_Camera.GetRight() * move.x + up * move.y + m_Camera.GetForward() * move.z) * speed;
    }

    void EditorCamera::SetAnglesKeepingPivot(f32 yaw, f32 pitch) {
        Vec3 pivot = GetPivot();
        m_Camera.Yaw = yaw;
        m_Camera.Pitch = glm::clamp(pitch, -kMaxPitch, kMaxPitch);
        m_Camera.Position = pivot - m_Camera.GetForward() * m_Distance;
    }

    void EditorCamera::Orbit(const Vec2& pixels) {
        SetAnglesKeepingPivot(m_Camera.Yaw + pixels.x * kTurnSpeed, m_Camera.Pitch - pixels.y * kTurnSpeed);
    }

    void EditorCamera::Pan(const Vec2& pixels, f32 viewportHeight) {
        f32 worldPerPixel = 2.0f * m_Distance * std::tan(m_Camera.FieldOfView * 0.5f) / std::max(viewportHeight, 1.0f);
        Vec3 right = m_Camera.GetRight();
        Vec3 up = glm::cross(right, m_Camera.GetForward());
        m_Camera.Position += (-right * pixels.x + up * pixels.y) * worldPerPixel;
    }

    void EditorCamera::Zoom(f32 steps) {
        // Each step covers a share of the distance left, so it never passes the pivot.
        Vec3 pivot = GetPivot();
        m_Distance = std::max(m_Distance * std::pow(0.85f, steps), kMinDistance);
        m_Camera.Position = pivot - m_Camera.GetForward() * m_Distance;
    }

    void EditorCamera::ChangeFlySpeed(f32 steps) {
        m_FlySpeed = glm::clamp(m_FlySpeed * std::pow(1.2f, steps), kMinFlySpeed, kMaxFlySpeed);
    }

    void EditorCamera::Focus(const Vec3& center, f32 radius) {
        f32 halfFov = m_Camera.FieldOfView * 0.5f;
        m_Distance = std::max(std::max(radius, 0.1f) / std::sin(halfFov) * 1.1f, kMinDistance);
        m_Camera.Position = center - m_Camera.GetForward() * m_Distance;
    }

} // namespace VKE::Editor
