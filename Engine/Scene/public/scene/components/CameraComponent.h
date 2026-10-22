#pragma once

#include <scene/Component.h>

#include <core/Types.h>
#include <renderer/RenderTypes.h>

namespace VKE::Scene {

    // A perspective view from its object, looking down the object's forward axis (-Z).
    // The first camera added to a scene becomes its main camera.
    class CameraComponent final : public Component {
    public:
        // Vertical, in radians.
        f32 FieldOfView = Renderer::kDefaultFieldOfView;
        f32 NearPlane = 0.1f;
        f32 FarPlane = 1000.0f;

        Renderer::RenderView GetView() const;
    };

} // namespace VKE::Scene
