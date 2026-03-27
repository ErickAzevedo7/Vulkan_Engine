// Behaviour.cpp
//
// Implements the Behaviour class methods that wrap engine internals
// (Transform, Entity). This file lives in the engine and is the only
// place that needs to include Transform.h and Entity.h from a script context.

#include "Behaviour.h"

// Internal engine headers — NOT exposed to script DLLs
#include <glm/gtc/quaternion.hpp>

#include "components/ScriptComponent.h"
#include "components/Transform.h"
#include "Entity.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
Behaviour::Behaviour(const char* name) : ScriptComponent(name) {
}

// ---------------------------------------------------------------------------
// Transform wrappers
// These are the only way scripts access world-space transform data.
// All calls go through owner->getComponent<Transform>() internally.
// ---------------------------------------------------------------------------

static Transform* getTransform(Entity* owner) {
	if (!owner)
		return nullptr;
	return owner->getComponent<Transform>();
}

glm::vec3 Behaviour::getPosition() const {
	if (auto* t = getTransform(owner))
		return t->getWorldPosition();
	return glm::vec3(0.0f);
}

void Behaviour::setPosition(glm::vec3 v) {
	if (auto* t = getTransform(owner))
		t->setWorldPosition(v);
}

glm::quat Behaviour::getRotation() const {
	if (auto* t = getTransform(owner))
		return t->getWorldRotation();
	return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
}

void Behaviour::setRotation(glm::quat q) {
	if (auto* t = getTransform(owner))
		t->setWorldRotation(q);
}

glm::vec3 Behaviour::getScale() const {
	if (auto* t = getTransform(owner))
		return t->getWorldScale();
	return glm::vec3(1.0f);
}

void Behaviour::setScale(glm::vec3 v) {
	if (auto* t = getTransform(owner))
		t->setWorldScale(v);
}
