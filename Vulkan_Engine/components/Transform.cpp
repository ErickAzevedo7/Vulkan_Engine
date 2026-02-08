#include "Transform.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"

Transform::Transform() {
}

Transform::~Transform() {
}

glm::mat4 Transform::getMatrix() const {
	glm::mat4 mat = glm::translate(glm::mat4(1.0f), position);
	mat *= glm::mat4_cast(rotation);
	mat = glm::scale(mat, scale);
	return mat;
}
