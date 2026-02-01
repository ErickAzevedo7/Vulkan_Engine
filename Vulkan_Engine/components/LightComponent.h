#pragma once

#include "Component.h"
#include "Entity.h"
#include <glm/glm.hpp>
#include "components/Transform.h"

enum class LightType { Directional = 0, Point = 1, Spot = 2 };

class LightComponent : public Component {
public:
	LightComponent(Entity* owner, LightType type = LightType::Point);
	~LightComponent();

	// Accessors
	LightType getType() const;
	void setType(LightType t);

	// Basic light properties (public for quick editor access)
	glm::vec3 color{1.0f, 1.0f, 1.0f};
	float intensity{1.0f};
	float range{10.0f}; // used by point/spot
	glm::vec3 direction{0.0f, -1.0f, 0.0f}; // used by directional/spot
	float innerConeAngle{glm::radians(12.5f)};
	float outerConeAngle{glm::radians(17.5f)};

	Entity* getOwner() const { return owner; }

	// Uniform layout for GPU upload (aligned)
	struct alignas(16) LightUniform {
		int type;
		glm::vec3 position; // world-space
		glm::vec3 direction;
		glm::vec3 color;
		float intensity;
		float range;
		float innerCone;
		float outerCone;
	};

	LightUniform getLightUniform() const;

private:
	Entity* owner{nullptr};
	LightType type{LightType::Point};
};
