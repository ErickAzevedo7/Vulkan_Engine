#pragma once
#include <vector>
#include "Entity.h"
#include <utility>

class Scene {
 public:
  Scene(std::string name);
  ~Scene();

  // Add a new entity to the scene
  Entity& createEntity(const std::string& name);

  // Remove an entity by index
  void removeEntity(size_t index);

  // Get a reference to an entity by index
  Entity& getEntity(size_t index);

  // Get the number of entities in the scene
  size_t getEntityCount() const;

  // Clear all entities from the scene
  void clear();

private:
  std::vector<Entity> entities;
  std::string name;
};
