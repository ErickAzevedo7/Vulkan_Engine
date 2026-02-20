#pragma once

#include "renderer/GraphicsDevice.h"
#include "vulkan/vulkan_core.h"


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
					VkSampleCountFlagBits msaaSamples);

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

private:
	VkDevice device = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	VkQueue presentQueue = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	uint32_t graphicsQueueFamilyIndex = 0;
	VkDeviceSize dynamicAlignmentValue = 0;
	VkSampleCountFlagBits msaaSamplesValue = VK_SAMPLE_COUNT_1_BIT;
};

} // namespace Renderer
