#include "renderer/Camera.h"

#include <cmath>

namespace VKE::Renderer {

    namespace {
        const Vec3 kWorldUp(0.0f, 1.0f, 0.0f);
    } // namespace

    Vec3 Camera::GetForward() const {
        return {std::sin(Yaw) * std::cos(Pitch), std::sin(Pitch), -std::cos(Yaw) * std::cos(Pitch)};
    }

    Vec3 Camera::GetRight() const {
        return glm::normalize(glm::cross(GetForward(), kWorldUp));
    }

    void Camera::LookAt(const Vec3& target) {
        Vec3 direction = target - Position;
        f32 length = glm::length(direction);
        if (length <= 0.0f) return;

        direction /= length;
        Pitch = std::asin(glm::clamp(direction.y, -1.0f, 1.0f));
        Yaw = std::atan2(direction.x, -direction.z);
    }

    RenderView Camera::GetView() const {
        RenderView view;
        view.View = glm::lookAt(Position, Position + GetForward(), kWorldUp);
        view.Position = Position;
        view.FieldOfView = FieldOfView;
        view.NearPlane = NearPlane;
        view.FarPlane = FarPlane;
        return view;
    }

} // namespace VKE::Renderer
