#include "ResourceContext.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "core/vulkancore.h"
#include "managers/LightManager.h"
#include "managers/MaterialManager.h"
#include "managers/MeshManager.h"
#include "managers/SceneManager.h"
#include "managers/TextureManager.h"
#include "renderer/vulkan/VulkanBuffer.h"
#include "renderer/vulkan/VulkanDevice.h"
#include "renderer/vulkan/VulkanResourceBinder.h"
#include "renderer/vulkan/VulkanTexture.h"
#include "vulkan/vulkan_core.h"

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

void ResourceContext::init(VulkanCore* engineCore) {
	// Initialize GraphicsDevice FIRST (wraps VulkanCore handles for injection)
	auto vulkanDevice = std::make_unique<Renderer::VulkanDevice>();
	// minImageCount represents the swapchain capability limits for ImGui init bounds
	SwapChainSupportDetails swapChainSupport = engineCore->querySwapChainSupport(VulkanCore::getPhysicalDevice());
	uint32_t minImageCount = swapChainSupport.capabilities.minImageCount;

	vulkanDevice->initialize(engineCore->getInstance(),
							 VulkanCore::getDevice(),
							 VulkanCore::getPhysicalDevice(),
							 VulkanCore::getGraphicsQueue(),
							 engineCore->getPresentQueue(),
							 VulkanCore::getCommandPool(),
							 engineCore->getGraphicsQueueFamily(),
							 VulkanCore::getDynamicAlignment(),
							 VulkanCore::getmsaaSamples(),
							 engineCore->getSwapChainImageFormat(),
							 VulkanCore::getSwapChainExtent(),
							 static_cast<uint32_t>(VulkanCore::getSwapChainImageViews().size()),
							 engineCore->findDepthFormat(),
							 engineCore->getPipeline(),
							 engineCore->getPipelineLayout(),
							 engineCore->getWindow(),
							 minImageCount);
	graphicsDevice = std::move(vulkanDevice);

	// Initialize buffer manager SECOND (Vulkan device is now ready)
	auto vulkanBuffer = std::make_unique<Renderer::VulkanBuffer>();
	vulkanBuffer->initialize(VulkanCore::getDevice(), VulkanCore::getPhysicalDevice());
	bufferManager = std::move(vulkanBuffer);

	// Initialize texture backend SECOND (needs device, queue, command pool)
	auto vulkanTexture = std::make_unique<Renderer::VulkanTexture>();
	vulkanTexture->initialize(VulkanCore::getDevice(),
							  VulkanCore::getPhysicalDevice(),
							  VulkanCore::getCommandPool(),
							  VulkanCore::getGraphicsQueue());
	graphicsTexture = std::move(vulkanTexture);

	// Initialize resource binder THIRD (needs device, buffer manager, and texture manager)
	auto vulkanBinder = std::make_unique<Renderer::VulkanResourceBinder>();
	vulkanBinder->initialize(VulkanCore::getDevice(), bufferManager.get(), graphicsTexture.get());

	// Create descriptor pool (moved from MaterialManager)
	// Allow many materials; each material needs MAX_FRAMES_IN_FLIGHT sets.
	const uint32_t maxMaterials = 1000;
	const uint32_t totalSets = maxMaterials * MAX_FRAMES_IN_FLIGHT;

	std::vector<VkDescriptorPoolSize> poolSizes;
	poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, totalSets});
	poolSizes.push_back(
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		 totalSets * 6}); // 6 samplers per set: texture + shadow2D + shadowCube + irradiance + prefilter + brdfLUT
	poolSizes.push_back({VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, totalSets * 2}); // Light + Material props

	vulkanBinder->createPool(totalSets * 2, poolSizes); // *2 for safety

	resourceBinder = std::move(vulkanBinder);

	// Set the buffer manager and binder for managers (deferred from constructor)
	lightManager->setBufferManager(bufferManager.get());
	materialManager->setBufferManager(bufferManager.get());
	materialManager->setResourceBinder(resourceBinder.get());
	materialManager->setLightManager(lightManager.get());
	textureManager->setGraphicsTexture(graphicsTexture.get());
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
	graphicsTexture.reset(); // Cleanup texture backend
	bufferManager.reset();
	graphicsDevice.reset(); // Cleanup device wrapper last
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

Renderer::GraphicsDevice& ResourceContext::getDevice() {
	return *graphicsDevice;
}
