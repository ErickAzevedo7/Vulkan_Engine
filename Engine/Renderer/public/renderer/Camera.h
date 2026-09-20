#pragma once

#include <core/Types.h>
#include <math/MathTypes.h>
#include <renderer/RenderTypes.h>

namespace VKE::Renderer {

    // Perspective camera described by a position and yaw/pitch angles.
    // Yaw 0 and pitch 0 look down -Z.
    class Camera {
    public:
        Vec3 Position{0.0f};
        // Radians.
        f32 Yaw = 0.0f;
        f32 Pitch = 0.0f;
        f32 FieldOfView = kDefaultFieldOfView;
        f32 NearPlane = 0.1f;
        f32 FarPlane = 1000.0f;

        Vec3 GetForward() const;
        Vec3 GetRight() const;

        void LookAt(const Vec3& target);

        RenderView GetView() const;
    };

} // namespace VKE::Renderer
