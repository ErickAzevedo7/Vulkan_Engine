#pragma once

#include "Component.h"

#include "glm/ext/vector_float3.hpp"

class ColliderComponent : public Component {
public:
	bool enabled = true;
	bool isTrigger = false;
	bool isStatic = false;
	glm::vec3 center = glm::vec3(0.0f);
	glm::vec3 size = glm::vec3(1.0f);

	ColliderComponent() = default;
	~ColliderComponent() override = default;
};
