#include "Entity.h"
#include "components/Component.h"
#include "components/Transform.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <string>

std::atomic<uint32_t> Entity::nextID{1};

Entity::Entity(std::string name) : id(nextID++) {
	this->name = name;
	addComponent(new Transform());
}

Entity::~Entity() {
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
