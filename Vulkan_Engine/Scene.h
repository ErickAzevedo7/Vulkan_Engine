#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "Entity.h"

enum class SceneState { Edit = 0, Play = 1 };

class Scene {
public:
	Scene(std::string name);
	~Scene();

	// Add a new entity to the scene
	Entity& createEntity(const std::string& name);

	// Remove an entity by index
	void removeEntity(size_t index);
	void removeEntityById(uint32_t id);

	// Get a reference to an entity by index
	Entity& getEntity(size_t index);

	// Get the number of entities in the scene
	size_t getEntityCount() const;

	// Find an entity by its unique ID, or nullptr if not found
	Entity* findEntityById(uint32_t id) const;

	// Clear all entities from the scene
	void clear();

	std::vector<std::unique_ptr<Entity>>* getEntities();

	// Dirty flag management
	bool getIsDirty() const;
	void markDirty();
	void clearDirty();

	// Runtime
	void onRuntimeStart();
	void onRuntimeStop();
	void onUpdate(float deltaTime);

	inline SceneState getState() const {
		return state;
	}
	inline void setState(SceneState s) {
		state = s;
	}

private:
	std::vector<std::unique_ptr<Entity>> entities;
	std::string name;
	bool isDirty = false;
	SceneState state = SceneState::Edit;
};
