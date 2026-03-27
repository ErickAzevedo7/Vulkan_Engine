#include "Entity.h"
#include "components/Component.h"
#include "components/Transform.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>

#include "glm/ext/matrix_float4x4.hpp"

std::atomic<uint32_t> Entity::nextID{1};

Entity::Entity(std::string name) : id(nextID++) {
	this->name = name;
	addComponent(new Transform());
}

Entity::~Entity() {
	clearParent(false);

	for (Entity* child : children) {
		if (child) {
			child->parent = nullptr;
		}
	}
	children.clear();

	for (Component* component : components) {
		delete component;
	}
	components.clear();
}

std::string Entity::getName() const {
	return name;
}

void Entity::addComponent(Component* component) {
	if (!component) {
		return;
	}

	if (dynamic_cast<Transform*>(component) && hasComponent<Transform>()) {
		delete component;
		return;
	}

	component->owner = this;
	components.push_back(component);
}

bool Entity::removeComponent(Component* component) {
	if (!component) {
		return false;
	}

	if (dynamic_cast<Transform*>(component)) {
		return false;
	}

	auto it = std::find(components.begin(), components.end(), component);
	if (it == components.end()) {
		return false;
	}

	(*it)->owner = nullptr;
	delete *it;
	components.erase(it);
	return true;
}

void Entity::setName(char* str) {
	name = std::string(str);
}

bool Entity::setParent(Entity* newParent, bool keepWorld) {
	if (newParent == this) {
		return false;
	}

	if (newParent && newParent->isDescendantOf(this)) {
		return false;
	}

	if (parent == newParent) {
		return true;
	}

	glm::mat4 worldMatrix(1.0f);
	auto* transform = getComponent<Transform>();
	if (keepWorld && transform) {
		worldMatrix = transform->getMatrix();
	}

	if (parent) {
		auto& siblings = parent->children;
		siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
	}

	parent = newParent;

	if (parent) {
		auto it = std::find(parent->children.begin(), parent->children.end(), this);
		if (it == parent->children.end()) {
			parent->children.push_back(this);
		}
	}

	if (keepWorld && transform) {
		transform->setFromWorldMatrix(worldMatrix);
	}

	return true;
}

void Entity::clearParent(bool keepWorld) {
	setParent(nullptr, keepWorld);
}

bool Entity::isDescendantOf(const Entity* potentialAncestor) const {
	if (!potentialAncestor) {
		return false;
	}

	const Entity* current = parent;
	while (current) {
		if (current == potentialAncestor) {
			return true;
		}
		current = current->parent;
	}

	return false;
}
