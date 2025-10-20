#include "Transform.h"

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
