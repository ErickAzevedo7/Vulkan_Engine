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
#include "context/ResourceContext.h"
#include "core/input/Input.h"
#include "Entity.h"
#include "managers/PrefabSerializer.h"
#include "Scene.h"
#include "ui/RuntimeHud.h"

namespace {
ResourceContext* s_resourceContext = nullptr;
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------
Behaviour::Behaviour(const char* name) : ScriptComponent(name) {
}

void Behaviour::setResourceContext(ResourceContext* context) {
	s_resourceContext = context;
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

bool Behaviour::keyDown(int keycode) const {
	return Core::Input::isKeyDown(keycode);
}

bool Behaviour::keyPressed(int keycode) const {
	return Core::Input::wasKeyPressed(keycode);
}

bool Behaviour::keyReleased(int keycode) const {
	return Core::Input::wasKeyReleased(keycode);
}

bool Behaviour::mouseDown(int button) const {
	return Core::Input::isMouseDown(button);
}

bool Behaviour::mousePressed(int button) const {
	return Core::Input::wasMousePressed(button);
}

bool Behaviour::mouseReleased(int button) const {
	return Core::Input::wasMouseReleased(button);
}

glm::vec2 Behaviour::mousePosition() const {
	return Core::Input::getMousePosition();
}

glm::vec2 Behaviour::mouseDelta() const {
	return Core::Input::getMouseDelta();
}

glm::vec2 Behaviour::scrollDelta() const {
	return Core::Input::getScrollDelta();
}

void Behaviour::hudText(const std::string& text,
						const glm::vec2& normalizedPosition,
						const glm::vec4& color,
						float scale,
						bool centered) const {
	RuntimeHud::addText(text, normalizedPosition, color, scale, centered);
}

void Behaviour::hudImage(const std::string& texturePath,
						 const glm::vec2& normalizedPosition,
						 const glm::vec2& normalizedSize,
						 const glm::vec4& tint,
						 bool centered) const {
	RuntimeHud::addImage(texturePath, normalizedPosition, normalizedSize, tint, centered);
}

Entity* Behaviour::instantiatePrefab(const std::string& prefabPath,
							 const std::string& rootNameOverride,
							 const glm::vec3& worldPosition,
							 const glm::quat& worldRotation,
							 bool applyWorldPosition,
							 bool applyWorldRotation) const {
	if (!owner || !s_resourceContext) {
		return nullptr;
	}

	Scene* scene = owner->getScene();
	if (!scene) {
		return nullptr;
	}

	Entity* spawned = PrefabSerializer::instantiate(prefabPath, scene, *s_resourceContext, rootNameOverride);
	if (!spawned) {
		return nullptr;
	}

	if (auto* transform = spawned->getComponent<Transform>()) {
		if (applyWorldPosition) {
			transform->setWorldPosition(worldPosition);
		}
		if (applyWorldRotation) {
			transform->setWorldRotation(worldRotation);
		}
	}

	return spawned;
}

void Behaviour::destroyOwner() const {
	if (!owner) {
		return;
	}

	Scene* scene = owner->getScene();
	if (!scene) {
		return;
	}

	scene->removeEntityById(owner->getID());
}

void Behaviour::destroyEntity(uint32_t entityId) const {
	if (!owner) {
		return;
	}

	Scene* scene = owner->getScene();
	if (!scene) {
		return;
	}

	scene->removeEntityById(entityId);
}
