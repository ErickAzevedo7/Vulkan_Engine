#pragma once

#include <cstdint>
#include <vector>

#include "vulkan/vulkan_core.h"

namespace Renderer {
class VulkanDevice;
}

/// Manages ImGui initialization, rendering, and cleanup
/// Handles all Vulkan-ImGui integration
class UIManager {
public:
	/// Initialize ImGui context, fonts, styling, and Vulkan integration
	/// @param vulkanDevice Pointer to VulkanDevice for accessing Vulkan resources
	/// @param swapchainImageViews List of active swapchain image views
	void init(Renderer::VulkanDevice* vulkanDevice, const std::vector<VkImageView>& swapchainImageViews);

	/// Begin a new ImGui frame
	/// Should be called at the start of each frame before rendering UI
	void beginFrame();

	/// Record ImGui rendering commands into a command buffer
	/// @param commandBuffer The command buffer to record into
	/// @param imageIndex The swap chain image index
	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	/// Get the ImGui command buffers
	std::vector<VkCommandBuffer>& getCommandBuffers() {
		return imGuiCommandBuffers;
	}

	/// @param swapchainImageViews Current swapchain image views
	void recreateFramebuffers(const std::vector<VkImageView>& swapchainImageViews);

	/// Cleanup all ImGui resources
	void cleanup();

private:
	Renderer::VulkanDevice* vulkanDevice = nullptr;
	VkResult err;

	VkCommandPool imGuiCommandPool = VK_NULL_HANDLE;
	std::vector<VkCommandBuffer> imGuiCommandBuffers;
	VkRenderPass imGuiRenderPass = VK_NULL_HANDLE;
	std::vector<VkFramebuffer> imGuiFramebuffers;
	VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;

	static void checkVkResult(VkResult err);
	void setupImGuiStyle();
	void createDescriptorPool();
	void createRenderPass();
	void createCommandPool();
	void createCommandBuffers(uint32_t imageCount);
	void createFramebuffers(const std::vector<VkImageView>& swapchainImageViews);
	void cleanupFramebuffers();
};
