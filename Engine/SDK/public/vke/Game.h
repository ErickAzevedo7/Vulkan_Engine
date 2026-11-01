#pragma once

#include <vke/Marks.h>
#include <vke/Math.h>
#include <vke/Object.h>

namespace vke {

    namespace Time {

        // Seconds since the last frame, the same OnUpdate receives.
        VKE_API float GetDeltaTime();
        // Seconds since the scene started playing. Doesn't count while paused.
        VKE_API float GetTime();
        // Frames since the scene started playing.
        VKE_API int GetFrame();

    } // namespace Time

    // Where a ray met something.
    struct RaycastHit {
        Object object;
        Vec3 point;
        // The direction the surface faces.
        Vec3 normal;
        // Meters from the start of the ray.
        float distance = 0.0f;
    };

    namespace Physics {

        // Looks along a line from `origin` for the nearest collider, and fills `hit` with it. False
        // if there's none within `maxDistance`. Triggers aren't seen, and neither is a collider
        // the line starts inside. Colliders are where they were at the end of the last frame.
        //
        //     vke::RaycastHit hit;
        //     if (vke::Physics::Raycast(eye, forward, hit, 20.0f)) vke::Destroy(hit.object);
        VKE_API bool Raycast(Vec3 origin, Vec3 direction, RaycastHit& hit, float maxDistance = 1000.0f);

    } // namespace Physics

    // Goes to another scene at the end of the frame. Every script gets OnStop.
    VKE_API void LoadScene(SceneRef scene);

    // Ends the game at the end of the frame. In the editor, it is the same as pressing Stop.
    VKE_API void Quit();

} // namespace vke
