#include "VulkanDevice.h"

#include <cstdint>

#include "vulkan/vulkan_core.h"


namespace Renderer {

void VulkanDevice::initialize(VkDevice device,
							  VkPhysicalDevice physicalDevice,
							  VkQueue graphicsQueue,
							  VkQueue presentQueue,
							  VkCommandPool commandPool,
							  uint32_t graphicsQueueFamily,
							  VkDeviceSize dynamicAlignment,
							  VkSampleCountFlagBits msaaSamples,
							  VkFormat swapchainImageFormat,
							  VkExtent2D swapchainExtent,
							  uint32_t swapchainImageCount,
							  VkFormat depthFormat,
							  VkPipeline pipeline,
							  VkPipelineLayout pipelineLayout,
							  GLFWwindow* window) {
	this->device = device;
	this->physicalDevice = physicalDevice;
	this->graphicsQueue = graphicsQueue;
	this->presentQueue = presentQueue;
	this->commandPool = commandPool;
	this->graphicsQueueFamilyIndex = graphicsQueueFamily;
	this->dynamicAlignmentValue = dynamicAlignment;
	this->msaaSamplesValue = msaaSamples;
	this->swapchainImageFormat = swapchainImageFormat;
	this->swapchainExtent = swapchainExtent;
	this->swapchainImageCount = swapchainImageCount;
	this->depthFormat = depthFormat;
	this->pipeline = pipeline;
	this->pipelineLayout = pipelineLayout;
	this->window = window;
}

void VulkanDevice::updateSwapchain(VkExtent2D newExtent, uint32_t newImageCount) {
	swapchainExtent = newExtent;
	swapchainImageCount = newImageCount;
}

void* VulkanDevice::getNativeDevice() const {
	return (void*)device;
}

void* VulkanDevice::getNativePhysicalDevice() const {
	return (void*)physicalDevice;
}

void* VulkanDevice::getNativeGraphicsQueue() const {
	return (void*)graphicsQueue;
}

void* VulkanDevice::getNativePresentQueue() const {
	return (void*)presentQueue;
}

void* VulkanDevice::getNativeCommandPool() const {
	return (void*)commandPool;
}

uint32_t VulkanDevice::getGraphicsQueueFamily() const {
	return graphicsQueueFamilyIndex;
}

uint64_t VulkanDevice::getDynamicAlignment() const {
	return static_cast<uint64_t>(dynamicAlignmentValue);
}

uint32_t VulkanDevice::getMsaaSamples() const {
	return static_cast<uint32_t>(msaaSamplesValue);
}

void VulkanDevice::waitIdle() {
	if (device != VK_NULL_HANDLE) {
		vkDeviceWaitIdle(device);
	}
}

} // namespace Renderer
