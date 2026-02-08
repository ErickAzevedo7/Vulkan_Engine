#include "ResourceContext.h"

#include "managers/LightManager.h"
#include "managers/MaterialManager.h"
#include "managers/TextureManager.h"


void ResourceContext::init() {
	// Initialize managers in dependency order
	// LightManager has no dependencies
	LightManager::init();

	// MaterialManager depends on LightManager (for descriptor sets)
	MaterialManager::init();
}

void ResourceContext::loadDefaults() {
	// Load default resources
	TextureManager::loadDefaults();
	TextureManager::loadAllFromAssets("assets");
	MaterialManager::loadDefault();
}

void ResourceContext::cleanup() {
	// Cleanup in reverse order of initialization
	MaterialManager::cleanup();
	TextureManager::cleanup();
	LightManager::cleanup();
}
