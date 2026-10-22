#pragma once

#include <scene/Component.h>

#include <core/Types.h>
#include <math/MathTypes.h>
#include <renderer/RenderTypes.h>

namespace VKE::Scene {

    // Everything about a light except where it is and where it points, which come from its object.
    struct LightSettings {
        Renderer::LightType Type = Renderer::LightType::Directional;
        Vec3 Color{1.0f};
        f32 Intensity = 1.0f;
        // Point and spot lights.
        f32 Range = 10.0f;
        // Spot lights, in radians from the center of the cone.
        f32 InnerConeAngle = glm::radians(20.0f);
        f32 OuterConeAngle = glm::radians(30.0f);
        bool CastsShadow = false;
    };

    // Lights the scene from its object's position, along the object's forward axis (-Z).
    class LightComponent final : public Component {
    public:
        explicit LightComponent(const LightSettings& settings = {});

        const LightSettings& GetSettings() const { return m_Settings; }
        void SetSettings(const LightSettings& settings);

    private:
        void ConnectToSystems(SceneSystems& systems) override;
        void SyncToSystems(SceneSystems& systems) override;
        void DisconnectFromSystems(SceneSystems& systems) override;

        LightSettings m_Settings;
        Renderer::LightID m_Light;
    };

} // namespace VKE::Scene
