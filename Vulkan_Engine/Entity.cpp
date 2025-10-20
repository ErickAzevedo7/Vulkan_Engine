#include "Entity.h"

Entity::Entity(std::string name) {
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
