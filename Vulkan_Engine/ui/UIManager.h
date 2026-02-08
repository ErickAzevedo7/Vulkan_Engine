#pragma once

#include <cstdint>
#include <vector>

#include "vulkan/vulkan_core.h"


// Forward declarations
class VulkanCore;

/// Manages ImGui initialization, rendering, and cleanup
/// Handles all Vulkan-ImGui integration
class UIManager {
public:
	/// Initialize ImGui context, fonts, styling, and Vulkan integration
	/// @param engineCore Pointer to VulkanCore for accessing Vulkan resources
	void init(VulkanCore* engineCore);

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

	/// Recreate framebuffers (called when swap chain is recreated)
	void recreateFramebuffers();

	/// Cleanup all ImGui resources
	void cleanup();

private:
	VulkanCore* engineCore = nullptr;
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
	void createCommandBuffers();
	void createFramebuffers();
	void cleanupFramebuffers();
};
