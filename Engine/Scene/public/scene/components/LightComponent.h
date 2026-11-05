#pragma once

#include <core/Types.h>
#include <math/MathTypes.h>
#include <renderer/RenderTypes.h>
#include <scene/Component.h>

namespace VKE {

// Lights the scene from its object's position, along the object's forward axis (-Z). The
// settings say everything else.
class LightComponent final : public Component {
	VKE_COMPONENT(LightComponent)

public:
	explicit LightComponent(const LightSettings &settings = {});

	const LightSettings &GetSettings() const { return m_Settings; }
	void SetSettings(const LightSettings &settings);

private:
	void ConnectToSystems(SceneSystems &systems) override;
	void SyncToSystems(SceneSystems &systems) override;
	void DisconnectFromSystems(SceneSystems &systems) override;

	VKE_PROPERTY() LightSettings m_Settings;
	LightID m_Light;
};

} // namespace VKE
