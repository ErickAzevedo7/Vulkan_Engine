#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "Scene.h"


class SceneManager {
public:
	SceneManager();
	~SceneManager();

	void loadDefaults();

	// Creates a new scene and returns its index
	size_t createScene(const std::string& name = "New Scene");

	// Removes a scene by index
	void removeScene(size_t index);

	// Returns a pointer to the scene at the given index
	Scene* getScene(size_t index);

	// Returns a pointer to the currently active scene
	Scene* getActiveScene();

	// Sets the active scene by index
	void setActiveScene(size_t index);

	// Returns the number of scenes managed
	size_t getSceneCount();

private:
	std::vector<std::unique_ptr<Scene>> scenes;
	size_t activeSceneIndex;
};
