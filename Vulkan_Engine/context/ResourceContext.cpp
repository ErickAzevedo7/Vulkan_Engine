#include "ResourceContext.h"

#include <memory>
#include <utility>

#include "core/vulkancore.h"
#include "managers/LightManager.h"
#include "managers/MaterialManager.h"
#include "managers/MeshManager.h"
#include "managers/SceneManager.h"
#include "managers/TextureManager.h"
#include "renderer/vulkan/VulkanBuffer.h"
#include "renderer/vulkan/VulkanResourceBinder.h"

ResourceContext::ResourceContext() {
	// Constructor - just allocate managers, defer Vulkan initialization
	// DO NOT initialize VulkanBuffer here - device is not ready yet!

	textureManager = std::make_unique<TextureManager>();
	materialManager = std::make_unique<MaterialManager>(*textureManager);
	// LightManager needs bufferManager, so we pass nullptr for now
	// It will be set in init() after bufferManager is created
	lightManager = std::make_unique<LightManager>(nullptr);
	sceneManager = std::make_unique<SceneManager>();
	meshManager = std::make_unique<MeshManager>();
}

void ResourceContext::init() {
	// Initialize buffer manager FIRST (Vulkan device is now ready)
	auto vulkanBuffer = std::make_unique<Renderer::VulkanBuffer>();
	vulkanBuffer->initialize(VulkanCore::getDevice(), VulkanCore::getPhysicalDevice());
	bufferManager = std::move(vulkanBuffer);

	// Initialize resource binder SECOND (needs device and buffer manager)
	auto vulkanBinder = std::make_unique<Renderer::VulkanResourceBinder>();
	vulkanBinder->initialize(VulkanCore::getDevice(), bufferManager.get());
	resourceBinder = std::move(vulkanBinder);

	// Set the buffer manager for managers (deferred from constructor)
	lightManager->setBufferManager(bufferManager.get());
	materialManager->setBufferManager(bufferManager.get());
	meshManager->setBufferManager(bufferManager.get());

	// Initialize managers that require Vulkan context (device/queues must be ready)
	// MaterialManager needs descriptor pool creation
	materialManager->init();
	// LightManager needs buffer creation
	lightManager->init();
}

void ResourceContext::cleanup() {
	// Explicitly release resources in reverse dependency order
	// Scene -> Light -> Material -> Texture -> Buffers
	// Using unique_ptr::reset() triggers destructors immediately
	sceneManager.reset();
	lightManager.reset();
	materialManager.reset();
	textureManager.reset();
	meshManager.reset();

	// Cleanup buffer manager last (RAII - destructor will call shutdown)
	// No need to cast - just reset the unique_ptr
	resourceBinder.reset(); // Cleanup binder before buffer manager
	bufferManager.reset();
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
	materialManager->loadAllFromAssets(); // Load material assets AFTER textures
	// MeshManager defaults loaded in main.cpp because of command pool dependency
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

MeshManager& ResourceContext::getMeshManager() {
	return *meshManager;
}

Renderer::GraphicsBuffer& ResourceContext::getBufferManager() {
	return *bufferManager;
}

Renderer::GraphicsResourceBinder& ResourceContext::getResourceBinder() {
	return *resourceBinder;
}
