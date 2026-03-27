#include "Scene.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
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

	removeEntityById(entities[index - 1]->getID());
}

void Scene::removeEntityById(uint32_t id) {
	Entity* root = findEntityById(id);
	if (!root) {
		return;
	}

	// Detach root from its own parent (notify parent it's losing a child)
	root->clearParent(true);

	std::vector<uint32_t> idsToRemove;
	idsToRemove.reserve(8);

	// Walk the subtree, severing child->parent links as we go
	std::function<void(Entity*)> collectSubtree = [&](Entity* current) {
		if (!current) {
			return;
		}

		idsToRemove.push_back(current->getID());
		for (Entity* child : current->getChildren()) {
			child->clearParent(false); // don't notify current, it's being deleted
			collectSubtree(child);
		}
	};

	collectSubtree(root);

	entities.erase(std::remove_if(entities.begin(),
								entities.end(),
								[&](const std::unique_ptr<Entity>& e) {
									if (!e) {
										return false;
									}
									for (uint32_t removeId : idsToRemove) {
										if (e->getID() == removeId) {
											return true;
										}
									}
									return false;
								}),
				 entities.end());

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
