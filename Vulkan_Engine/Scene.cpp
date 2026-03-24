#include "Scene.h"

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Entity.h"
#include "components/ScriptComponent.h"

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
			if (entity->getName() == uniqueName) {
				nameExists = true;
				uniqueName = name + "(" + std::to_string(counter++) + ")";
				break;
			}
		}
	}

	entities.push_back(std::make_unique<Entity>(uniqueName));
	markDirty();
	return *entities.back();
}

void Scene::removeEntity(size_t index) {
	if (index == 0 || index > entities.size()) {
		throw std::out_of_range("Entity index out of range");
	}
	entities.erase(entities.begin() + (index - 1));
	markDirty();
}

Entity& Scene::getEntity(size_t index) {
	if (index == 0 || index > entities.size()) {
		throw std::out_of_range("Entity index out of range");
	}
	return *entities[index - 1];
}

size_t Scene::getEntityCount() const {
	return entities.size();
}

Entity* Scene::findEntityById(uint32_t id) const {
	for (const auto& entity : entities) {
		if (entity->getID() == id) {
			return entity.get();
		}
	}
	return nullptr;
}

void Scene::clear() {
	entities.clear();
	markDirty();
}

std::vector<std::unique_ptr<Entity>>* Scene::getEntities() {
	return &entities;
}

bool Scene::getIsDirty() const {
	return isDirty;
}

void Scene::markDirty() {
	isDirty = true;
}

void Scene::clearDirty() {
	isDirty = false;
}

void Scene::onRuntimeStart() {
	state = SceneState::Play;
	for (const auto& ePtr : entities) {
		auto scripts = ePtr->getComponents<ScriptComponent>();
		for (auto* sc : scripts) {
			sc->onStart();
		}
	}
}

void Scene::onRuntimeStop() {
	for (const auto& ePtr : entities) {
		auto scripts = ePtr->getComponents<ScriptComponent>();
		for (auto* sc : scripts) {
			sc->onStop();
		}
	}
	state = SceneState::Edit;
}

void Scene::onUpdate(float deltaTime) {
	if (state == SceneState::Play) {
		for (const auto& ePtr : entities) {
			auto scripts = ePtr->getComponents<ScriptComponent>();
			for (auto* sc : scripts) {
				sc->onUpdate(deltaTime);
			}
		}
	}
}
