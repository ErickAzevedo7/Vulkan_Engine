#include "Entity.h"
#include "components/Component.h"

#include <atomic>
#include <cstdint>
#include <string>

std::atomic<uint32_t> Entity::nextID{1};

Entity::Entity(std::string name) : id(nextID++) {
	this->name = name;
}

Entity::~Entity() {
}

std::string Entity::getName() const {
	return name;
}

void Entity::addComponent(Component* component) {
	if (component) {
		component->owner = this;
	}
	components.push_back(component);
}

void Entity::setName(char* str) {
	name = std::string(str);
}
