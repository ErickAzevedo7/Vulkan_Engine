#include "SceneManager.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "Scene.h"


SceneManager::SceneManager() : activeSceneIndex(0) {
	// Initialize defaults immediately or defer to explicit loadDefaults()
}

SceneManager::~SceneManager() {
	scenes.clear();
}

void SceneManager::loadDefaults() {
	createScene("Default Scene");
	activeSceneIndex = 0;
}

size_t SceneManager::createScene(const std::string& name) {
	scenes.emplace_back(std::make_unique<Scene>(name));
	activeSceneIndex = scenes.size() - 1;
	return activeSceneIndex;
}

void SceneManager::removeScene(const size_t index) {
	if (index < scenes.size()) {
		scenes.erase(scenes.begin() + index);
		// Adjust activeSceneIndex if necessary
		if (activeSceneIndex >= scenes.size()) {
			activeSceneIndex = !scenes.empty() ? scenes.size() - 1 : 0;
		}
	}
}

Scene* SceneManager::getScene(size_t index) {
	if (index < scenes.size()) {
		return scenes[index].get();
	}
	return nullptr;
}

Scene* SceneManager::getActiveScene() {
	if (!scenes.empty()) {
		return scenes[activeSceneIndex].get();
	}
	return nullptr;
}

void SceneManager::setActiveScene(const size_t index) {
	if (index < scenes.size()) {
		activeSceneIndex = index;
	}
}

size_t SceneManager::getSceneCount() {
	return scenes.size();
}
