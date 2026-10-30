#pragma once

#include <core/Types.h>
#include <math/MathTypes.h>
#include <renderer/Camera.h>
#include <renderer/RenderTypes.h>

namespace VKE::Editor {

    // The camera the Viewport shows the scene from, apart from the scene's own cameras and never
    // saved with it. It turns around a point in front of it, which flying carries along,
    // orbiting turns around, panning slides and zooming comes closer to.
    class EditorCamera {
    public:
        EditorCamera();

        // Looks from `position` at `target`, which becomes the point to orbit.
        void LookAt(const Vec3& position, const Vec3& target);

        // Turns by `pixels` of mouse motion and moves by `move` (x right, y up, z forward, each
        // -1 to 1) for `deltaTime` seconds.
        void Fly(const Vec2& pixels, const Vec3& move, f32 deltaTime, bool fast);
        void Orbit(const Vec2& pixels);
        // `viewportHeight` makes the scene follow the mouse at the orbit point's depth.
        void Pan(const Vec2& pixels, f32 viewportHeight);
        // Steps of the mouse wheel; positive comes closer.
        void Zoom(f32 steps);
        // Flying faster or slower, by steps of the mouse wheel.
        void ChangeFlySpeed(f32 steps);
        // Frames a sphere: looks at its center from far enough to see all of it.
        void Focus(const Vec3& center, f32 radius);

        Renderer::RenderView GetView() const { return m_Camera.GetView(); }
        Vec3 GetPosition() const { return m_Camera.Position; }
        Vec3 GetForward() const { return m_Camera.GetForward(); }
        f32 GetFlySpeed() const { return m_FlySpeed; }

    private:
        Vec3 GetPivot() const { return m_Camera.Position + m_Camera.GetForward() * m_Distance; }
        void SetAnglesKeepingPivot(f32 yaw, f32 pitch);

        Renderer::Camera m_Camera;
        // How far in front the orbit point is.
        f32 m_Distance = 8.0f;
        // Meters per second.
        f32 m_FlySpeed = 5.0f;
    };

} // namespace VKE::Editor
