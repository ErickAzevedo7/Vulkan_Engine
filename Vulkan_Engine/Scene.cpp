#include "Scene.h"

#include <utility>

Scene::Scene(std::string name) {
  this->name = std::move(name);
}

Scene::~Scene() {
  return;
}

Entity& Scene::createEntity(const std::string& name) {
  std::string uniqueName = name;
  int counter = 1;
  bool nameExists = true;

  while (nameExists) {
	  nameExists = false;
    for (const auto& entity : entities) {
      if (entity.getName() == uniqueName) {
        nameExists = true;
        uniqueName = name + "(" + std::to_string(counter++) + ")";
        break;
      }
    }
  }

  entities.emplace_back(uniqueName);
  return entities.back();
}

void Scene::removeEntity(size_t index) {
  if (index >= entities.size()) {
    throw std::out_of_range("Entity index out of range");
  }
  entities.erase(entities.begin() + index);
}

Entity& Scene::getEntity(size_t index) {
  if (index >= entities.size()) {
    throw std::out_of_range("Entity index out of range");
  }
  return entities[index];
}

size_t Scene::getEntityCount() const {
  return entities.size();
}

void Scene::clear() {
  entities.clear();
}
