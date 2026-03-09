#include "CameraComponent.h"

#include <glm/gtc/matrix_transform.hpp>

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/trigonometric.hpp"


CameraComponent::CameraComponent(Entity* parent) : owner(parent) {
}

CameraComponent::~CameraComponent() {
}

glm::mat4 CameraComponent::getProjectionMatrix(float aspectRatio) const {
	glm::mat4 proj = glm::perspective(glm::radians(fov), aspectRatio, nearPlane, farPlane);
	// Vulkan inverted Y
	proj[1][1] *= -1;
	return proj;
}
