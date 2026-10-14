#pragma once

#include <core/Types.h>
#include <math/MathTypes.h>

#include <array>

namespace VKE::Renderer {

    // Cameras that render the six faces of a cube texture, in the order the layers
    // are stored: +X, -X, +Y, -Y, +Z, -Z.
    //
    // The cube sampler reads a face's rows top to bottom the way the RHI draws them
    // with Y up, but its columns run opposite to a right-handed camera's right axis.
    // The projection mirrors X so each texel lands where the sampler looks for it.
    inline Mat4 GetCubeFaceProjection(f32 nearPlane, f32 farPlane) {
        Mat4 projection = glm::perspectiveRH_ZO(glm::radians(90.0f), 1.0f, nearPlane, farPlane);
        projection[0][0] *= -1.0f;
        return projection;
    }

    inline std::array<Mat4, 6> GetCubeFaceViews(const Vec3& origin) {
        const Vec3 up(0.0f, 1.0f, 0.0f);
        return {
            glm::lookAtRH(origin, origin + Vec3(1.0f, 0.0f, 0.0f), up),
            glm::lookAtRH(origin, origin + Vec3(-1.0f, 0.0f, 0.0f), up),
            glm::lookAtRH(origin, origin + Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, -1.0f)),
            glm::lookAtRH(origin, origin + Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f)),
            glm::lookAtRH(origin, origin + Vec3(0.0f, 0.0f, 1.0f), up),
            glm::lookAtRH(origin, origin + Vec3(0.0f, 0.0f, -1.0f), up),
        };
    }

} // namespace VKE::Renderer
