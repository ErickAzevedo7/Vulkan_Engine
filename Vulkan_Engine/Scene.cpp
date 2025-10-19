#include "Scene.h"

Scene::Scene(std::string name) {
  this->name = name;
}

Scene::~Scene() {
  return;
}

Entity& Scene::createEntity(std::string name) {
  entities.emplace_back(name);
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
