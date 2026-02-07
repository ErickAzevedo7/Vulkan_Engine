#include "LightComponent.h"
#include "components/Transform.h"

LightComponent::LightComponent(Entity* owner, LightType type)
	: Component(), owner(owner), type(type) {
}

LightComponent::~LightComponent() {
}

LightType LightComponent::getType() const { return type; }

void LightComponent::setType(LightType t) { type = t; }

LightComponent::LightUniform LightComponent::getLightUniform() const {
	LightUniform u{};
	u.type = static_cast<int>(type);

	// try to fetch position from owner's transform if available
	if (owner) {
		if (auto tcomp = owner->getComponent<Transform>()) {
			u.position = tcomp->position;
			// forward direction by transform rotation
			u.direction = tcomp->rotation * direction;
		}
		else {
			u.position = glm::vec3(0.0f);
			u.direction = direction;
		}
	}
	else {
		u.position = glm::vec3(0.0f);
		u.direction = direction;
	}

	u.color = color;
	u.intensity = intensity;
	u.range = range;
	u.innerCone = innerConeAngle;
	u.outerCone = outerConeAngle;
	u.ambient = ambient;
	u.diffuse = diffuse;
	u.specular = specular;
	u.attenuationKc = attenuationKc;
	u.attenuationKl = attenuationKl;
	u.attenuationKq = attenuationKq;
	u.useBlinnPhong = useBlinnPhong ? 1 : 0;

	return u;
}
