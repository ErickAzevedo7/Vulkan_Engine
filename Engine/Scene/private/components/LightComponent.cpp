#include "scene/components/LightComponent.h"
#include "SceneSystems.h"
#include "scene/GameObject.h"

namespace VKE::Scene {

    LightComponent::LightComponent(const LightSettings& settings)
        : m_Settings(settings) {
    }

    void LightComponent::SetSettings(const LightSettings& settings) {
        m_Settings = settings;
        MarkSyncNeeded();
    }

    void LightComponent::ConnectToSystems(SceneSystems&) {
        // The light enters the render scene on the first sync, if its object is active.
        MarkSyncNeeded();
    }

    void LightComponent::SyncToSystems(SceneSystems& systems) {
        // The render scene has no hidden lights, so an inactive light leaves it.
        if (!GetOwner().IsActiveInHierarchy()) {
            DisconnectFromSystems(systems);
            return;
        }

        Renderer::LightDesc desc;
        desc.Type = m_Settings.Type;
        desc.Color = m_Settings.Color;
        desc.Intensity = m_Settings.Intensity;
        desc.Range = m_Settings.Range;
        desc.InnerConeAngle = m_Settings.InnerConeAngle;
        desc.OuterConeAngle = m_Settings.OuterConeAngle;
        desc.CastsShadow = m_Settings.CastsShadow;
        const Transform& world = GetTransform().GetWorld();
        desc.Position = world.Position;
        desc.Direction = world.GetForward();

        if (m_Light.IsValid()) {
            systems.Render.SetLight(m_Light, desc);
        } else {
            m_Light = systems.Render.CreateLight(desc);
        }
    }

    void LightComponent::DisconnectFromSystems(SceneSystems& systems) {
        if (m_Light.IsValid()) systems.Render.DestroyLight(m_Light);
        m_Light = {};
    }

} // namespace VKE::Scene
