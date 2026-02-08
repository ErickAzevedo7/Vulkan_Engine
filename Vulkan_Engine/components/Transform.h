#pragma once

#include "Component.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/quaternion_float.hpp"
#include "glm/ext/vector_float3.hpp"

class Transform : public Component {
public:
	glm::vec3 position{0.0f, 0.0f, 0.0f};
	glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
	glm::vec3 scale{1.0f, 1.0f, 1.0f};

	Transform();
	~Transform();

	glm::mat4 getMatrix() const;
};
