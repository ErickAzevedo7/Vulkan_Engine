#include "ResourceContext.h"

#include <memory>

#include "managers/LightManager.h"
#include "managers/MaterialManager.h"
#include "managers/SceneManager.h"
#include "managers/TextureManager.h"

// Initialize static members
std::unique_ptr<LightManager> ResourceContext::lightManager;
std::unique_ptr<MaterialManager> ResourceContext::materialManager;
std::unique_ptr<TextureManager> ResourceContext::textureManager;
std::unique_ptr<SceneManager> ResourceContext::sceneManager;

void ResourceContext::init() {
	// Create Managers after Vulkan is initialized
	// Constructor handles all initialization (true RAII!)
	textureManager = std::make_unique<TextureManager>();
	materialManager = std::make_unique<MaterialManager>();
	lightManager = std::make_unique<LightManager>();
	sceneManager = std::make_unique<SceneManager>();
}

void ResourceContext::loadDefaults() {
	textureManager->loadDefaults();
	textureManager->loadAllFromAssets("assets");
	materialManager->loadDefault();
	sceneManager->loadDefaults();
}

void ResourceContext::cleanup() {
	// Manager cleanup handled automatically by unique_ptr destructor
	lightManager.reset();
	materialManager.reset();
	textureManager.reset();
	sceneManager.reset();
}

LightManager& ResourceContext::getLightManager() {
	return *lightManager;
}

MaterialManager& ResourceContext::getMaterialManager() {
	return *materialManager;
}

TextureManager& ResourceContext::getTextureManager() {
	return *textureManager;
}

SceneManager& ResourceContext::getSceneManager() {
	return *sceneManager;
}
