#include "ResourceContext.h"

#include <memory>

#include "managers/LightManager.h"
#include "managers/MaterialManager.h"
#include "managers/TextureManager.h"

// Initialize pointer to nullptr
std::unique_ptr<LightManager> ResourceContext::lightManager = nullptr;
std::unique_ptr<MaterialManager> ResourceContext::materialManager = nullptr;

void ResourceContext::init() {
	// Create Managers after Vulkan is initialized
	// Constructor handles all initialization (true RAII!)
	materialManager = std::make_unique<MaterialManager>();
	lightManager = std::make_unique<LightManager>();
}

void ResourceContext::loadDefaults() {
	TextureManager::loadDefaults();
	TextureManager::loadAllFromAssets("assets");
	materialManager->loadDefault();
}

void ResourceContext::cleanup() {
	// Manager cleanup handled automatically by unique_ptr destructor
	lightManager.reset();
	materialManager.reset();
	TextureManager::cleanup();
}

LightManager& ResourceContext::getLightManager() {
	return *lightManager;
}

MaterialManager& ResourceContext::getMaterialManager() {
	return *materialManager;
}
