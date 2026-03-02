#include "LightComponent.h"

#include "components/Transform.h"
#include "Entity.h"

#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/trigonometric.hpp"

LightComponent::LightComponent(Entity* owner, LightType type) : Component(), owner(owner), type(type) {
}

LightComponent::~LightComponent() {
}

LightType LightComponent::getType() const {
	return type;
}

void LightComponent::setType(LightType t) {
	type = t;
}

LightComponent::LightUniform LightComponent::getLightUniform() const {
	LightUniform u{};
	glm::vec3 dir = direction;
	glm::vec3 pos = glm::vec3(0.0f);

	// try to fetch position from owner's transform if available
	if (owner) {
		if (auto tcomp = owner->getComponent<Transform>()) {
			pos = tcomp->position;
			// forward direction by transform rotation
			dir = tcomp->rotation * direction;
		}
	}

	u.colorIntensity = glm::vec4(color, intensity);
	u.direction = glm::vec4(dir, 0.0f);
	u.positionType = glm::vec4(pos, static_cast<float>(static_cast<int>(type)));
	u.ambient = glm::vec4(ambient, 0.0f);
	u.diffuse = glm::vec4(diffuse, 0.0f);
	u.specular = glm::vec4(specular, 0.0f);
	u.attenuationKc = attenuationKc;
	u.attenuationKl = attenuationKl;
	u.attenuationKq = attenuationKq;
	u.cutOff = glm::cos(innerConeAngle);
	u.outerCutOff = glm::cos(outerConeAngle);
	u.useBlinnPhong = useBlinnPhong ? 1 : 0;
	// far_plane: distance at which the light's shadow map and attenuation saturates
	u.far_plane = range;

	return u;
}
