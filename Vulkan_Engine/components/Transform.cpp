#include "Transform.h"

#include "Entity.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtx/matrix_decompose.hpp"
#include "glm/gtc/quaternion.hpp"

Transform::Transform() {
}

Transform::~Transform() {
}

glm::mat4 Transform::getLocalMatrix() const {
	glm::mat4 mat = glm::translate(glm::mat4(1.0f), position);
	mat *= glm::mat4_cast(rotation);
	mat = glm::scale(mat, scale);
	return mat;
}

glm::mat4 Transform::getMatrix() const {
	glm::mat4 local = getLocalMatrix();
	if (!owner) {
		return local;
	}

	Entity* parent = owner->getParent();
	if (!parent) {
		return local;
	}

	Transform* parentTransform = parent->getComponent<Transform>();
	if (!parentTransform) {
		return local;
	}

	return parentTransform->getMatrix() * local;
}

glm::vec3 Transform::getWorldPosition() const {
	return glm::vec3(getMatrix()[3]);
}

glm::quat Transform::getWorldRotation() const {
	glm::vec3 skew(0.0f);
	glm::vec4 perspective(0.0f);
	glm::vec3 outScale(1.0f);
	glm::vec3 outTranslation(0.0f);
	glm::quat outRotation(1.0f, 0.0f, 0.0f, 0.0f);
	glm::decompose(getMatrix(), outScale, outRotation, outTranslation, skew, perspective);
	return outRotation;
}

glm::vec3 Transform::getWorldScale() const {
	glm::vec3 skew(0.0f);
	glm::vec4 perspective(0.0f);
	glm::vec3 outScale(1.0f);
	glm::vec3 outTranslation(0.0f);
	glm::quat outRotation(1.0f, 0.0f, 0.0f, 0.0f);
	glm::decompose(getMatrix(), outScale, outRotation, outTranslation, skew, perspective);
	return outScale;
}

void Transform::setWorldPosition(const glm::vec3& worldPosition) {
	glm::mat4 world = getMatrix();
	world[3] = glm::vec4(worldPosition, 1.0f);
	setFromWorldMatrix(world);
}

void Transform::setWorldRotation(const glm::quat& worldRotation) {
	glm::vec3 worldScale = getWorldScale();
	glm::vec3 worldPosition = getWorldPosition();

	glm::mat4 world = glm::translate(glm::mat4(1.0f), worldPosition);
	world *= glm::mat4_cast(glm::normalize(worldRotation));
	world = glm::scale(world, worldScale);

	setFromWorldMatrix(world);
}

void Transform::setWorldScale(const glm::vec3& worldScale) {
	glm::quat worldRotation = getWorldRotation();
	glm::vec3 worldPosition = getWorldPosition();

	glm::mat4 world = glm::translate(glm::mat4(1.0f), worldPosition);
	world *= glm::mat4_cast(worldRotation);
	world = glm::scale(world, worldScale);

	setFromWorldMatrix(world);
}

void Transform::setFromWorldMatrix(const glm::mat4& worldMatrix) {
	glm::mat4 localMatrix = worldMatrix;
	if (owner) {
		if (Entity* parent = owner->getParent()) {
			if (Transform* parentTransform = parent->getComponent<Transform>()) {
				localMatrix = glm::inverse(parentTransform->getMatrix()) * worldMatrix;
			}
		}
	}

	glm::vec3 skew(0.0f);
	glm::vec4 perspective(0.0f);
	glm::vec3 outScale(1.0f);
	glm::vec3 outTranslation(0.0f);
	glm::quat outRotation(1.0f, 0.0f, 0.0f, 0.0f);
	glm::decompose(localMatrix, outScale, outRotation, outTranslation, skew, perspective);

	position = outTranslation;
	rotation = glm::normalize(outRotation);
	scale = outScale;
}
