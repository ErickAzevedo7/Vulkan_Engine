#include "Entity.h"

std::atomic<uint32_t> Entity::nextID{0};

Entity::Entity(std::string name): id(nextID++) {
  this->name = name;
}

Entity::~Entity() {
}

std::string Entity::getName() const {
  return name;
}

void Entity::addComponent(Component* component) {
  components.push_back(component);
}

void Entity::setName(char* str) {
  name = std::string(str);
}
