#pragma once

#include <glm/glm.hpp>

#include "Component.h"

#include "glm/ext/matrix_float4x4.hpp"


class Entity;

class CameraComponent : public Component {
public:
	CameraComponent(Entity* parent);
	~CameraComponent();

	Entity* owner{nullptr};

	float fov = 45.0f;
	float nearPlane = 0.1f;
	float farPlane = 1000.0f;
	bool isPrimary = true;

	glm::mat4 getProjectionMatrix(float aspectRatio) const;
};
