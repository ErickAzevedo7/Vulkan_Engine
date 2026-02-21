#pragma once

#include <cstdint>

#include "renderer/GraphicsDevice.h"
#include "vulkan/vulkan_core.h"


struct GLFWwindow;

namespace Renderer {

/**
 * @brief Vulkan implementation of GraphicsDevice.
 * Initially delegates to VulkanCore static members, but owns
 * no Vulkan resources — VulkanCore still manages the lifecycle.
 */
class VulkanDevice : public GraphicsDevice {
public:
	VulkanDevice() = default;
	~VulkanDevice() override = default;

	void initialize(VkDevice device,
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
					GLFWwindow* window);

	/** Call after swapchain recreation to update extent/image count. */
	void updateSwapchain(VkExtent2D newExtent, uint32_t newImageCount);

	// --- GraphicsDevice interface ---

	void* getNativeDevice() const override;
	void* getNativePhysicalDevice() const override;
	void* getNativeGraphicsQueue() const override;
	void* getNativePresentQueue() const override;
	void* getNativeCommandPool() const override;
	uint32_t getGraphicsQueueFamily() const override;
	uint64_t getDynamicAlignment() const override;
	uint32_t getMsaaSamples() const override;
	void waitIdle() override;

	// --- Convenience typed accessors (Vulkan-specific, for internal use) ---

	VkDevice getDevice() const {
		return device;
	}
	VkPhysicalDevice getPhysicalDevice() const {
		return physicalDevice;
	}
	VkQueue getGraphicsQueue() const {
		return graphicsQueue;
	}
	VkQueue getPresentQueue() const {
		return presentQueue;
	}
	VkCommandPool getCommandPool() const {
		return commandPool;
	}

	// --- Swapchain / presentation queries ---

	VkFormat getSwapChainImageFormat() const {
		return swapchainImageFormat;
	}
	VkExtent2D getSwapChainExtent() const {
		return swapchainExtent;
	}
	uint32_t getSwapChainImageCount() const {
		return swapchainImageCount;
	}
	VkFormat findDepthFormat() const {
		return depthFormat;
	}
	VkPipeline getPipeline() const {
		return pipeline;
	}
	VkPipelineLayout getPipelineLayout() const {
		return pipelineLayout;
	}

	// --- Window / uniform buffer access ---

	GLFWwindow* getWindow() const {
		return window;
	}

private:
	VkDevice device = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue presentQueue = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	uint32_t graphicsQueueFamilyIndex = 0;
	VkDeviceSize dynamicAlignmentValue = 0;
	VkSampleCountFlagBits msaaSamplesValue = VK_SAMPLE_COUNT_1_BIT;

	// Swapchain / presentation state
	VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D swapchainExtent = {0, 0};
	uint32_t swapchainImageCount = 0;
	VkFormat depthFormat = VK_FORMAT_UNDEFINED;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	// Window state
	GLFWwindow* window = nullptr;
};

} // namespace Renderer
