#pragma once

#include "Component.h"

#include "glm/ext/vector_float3.hpp"

class StaticMeshColliderComponent : public Component {
public:
	bool enabled = true;
	bool isTrigger = false;
	bool useAttachedMeshBounds = true;
	glm::vec3 localCenter = glm::vec3(0.0f);
	glm::vec3 localSize = glm::vec3(1.0f);

	StaticMeshColliderComponent() = default;
	~StaticMeshColliderComponent() override = default;
};
