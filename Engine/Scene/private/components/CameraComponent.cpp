#include "scene/components/CameraComponent.h"
#include "scene/GameObject.h"

namespace VKE::Scene {

    Renderer::RenderView CameraComponent::GetView() const {
        // Built from the rotation alone, so scale on the object doesn't stretch the view.
        const Transform& world = GetTransform().GetWorld();
        Vec3 position = world.Position;
        Vec3 forward = world.GetForward();
        Vec3 up = world.GetUp();

        Renderer::RenderView view;
        view.View = glm::lookAt(position, position + forward, up);
        view.Position = position;
        view.FieldOfView = FieldOfView;
        view.NearPlane = NearPlane;
        view.FarPlane = FarPlane;
        return view;
    }

} // namespace VKE::Scene
