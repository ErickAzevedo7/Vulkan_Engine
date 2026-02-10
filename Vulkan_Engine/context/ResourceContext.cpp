#include "ResourceContext.h"

#include <memory>

#include "managers/LightManager.h"
#include "managers/MaterialManager.h"
#include "managers/SceneManager.h"
#include "managers/TextureManager.h"

ResourceContext::ResourceContext() {
	// Initialize instances
	// Constructor handles allocation, but Vulkan initialization is deferred
	textureManager = std::make_unique<TextureManager>();
	materialManager = std::make_unique<MaterialManager>(*textureManager);
	lightManager = std::make_unique<LightManager>();
	sceneManager = std::make_unique<SceneManager>();
}

void ResourceContext::init() {
	// Initialize managers that require Vulkan context (device/queues must be ready)
	// MaterialManager needs descriptor pool creation
	materialManager->init();
	// LightManager needs buffer creation
	lightManager->init();
}

void ResourceContext::cleanup() {
	// Explicitly release resources in reverse dependency order
	// Scene -> Light -> Material -> Texture
	// Using unique_ptr::reset() triggers destructors immediately
	sceneManager.reset();
	lightManager.reset();
	materialManager.reset();
	textureManager.reset();
}

ResourceContext::~ResourceContext() {
	// Manager cleanup handled automatically by unique_ptr destructor
	// Order: Scene -> Light -> Material -> Texture (reverse of declaration/creation typically)
	// If cleanup() was called manually, pointers are already null and this does nothing.
	cleanup();
}

void ResourceContext::loadDefaults() {
	textureManager->loadDefaults();
	textureManager->loadAllFromAssets("assets");
	materialManager->loadDefault();
	sceneManager->loadDefaults();
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
