#include "Entity.h"

Entity::Entity(std::string name)
{
  this->name = name;
}

Entity::~Entity()
{
	return;
}

std::string Entity::getName() const {
  return name;
}
