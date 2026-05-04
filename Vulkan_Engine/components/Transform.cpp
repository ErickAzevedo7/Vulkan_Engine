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
	if (!owner) {
		position = worldPosition;
		return;
	}
	Entity* parent = owner->getParent();
	if (!parent) {
		position = worldPosition;
		return;
	}
	Transform* parentTransform = parent->getComponent<Transform>();
	if (!parentTransform) {
		position = worldPosition;
		return;
	}
	
	glm::mat4 parentInv = glm::inverse(parentTransform->getMatrix());
	glm::vec4 localPos = parentInv * glm::vec4(worldPosition, 1.0f);
	position = glm::vec3(localPos) / localPos.w;
}

void Transform::setWorldRotation(const glm::quat& worldRotation) {
	if (!owner) {
		rotation = worldRotation;
		return;
	}
	Entity* parent = owner->getParent();
	if (!parent) {
		rotation = worldRotation;
		return;
	}
	Transform* parentTransform = parent->getComponent<Transform>();
	if (!parentTransform) {
		rotation = worldRotation;
		return;
	}
	
	glm::quat parentRotInv = glm::inverse(parentTransform->getWorldRotation());
	rotation = parentRotInv * worldRotation;
}

void Transform::setWorldScale(const glm::vec3& worldScale) {
	if (!owner) {
		scale = worldScale;
		return;
	}
	Entity* parent = owner->getParent();
	if (!parent) {
		scale = worldScale;
		return;
	}
	Transform* parentTransform = parent->getComponent<Transform>();
	if (!parentTransform) {
		scale = worldScale;
		return;
	}
	
	glm::vec3 parentScale = parentTransform->getWorldScale();
	scale = worldScale / glm::max(parentScale, glm::vec3(0.0001f));
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
