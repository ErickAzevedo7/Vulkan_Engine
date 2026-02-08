#pragma once
#include "core/vulkancore.h"
#include "vulkan/vulkan_core.h"

#include <cstdint>
#include <vector>

class MousePick {
public:
	VkExtent2D mousePickExtent;
	std::vector<VkImageView> mousePickImageViews;
	std::vector<VkCommandBuffer> mousePickCommandBuffers;

	void init(VulkanCore* core);
	void recordMousePickCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
	void cleanupFramebuffers();
	void recreateMousePick();

	uint32_t getEntityIDAt(int x, int y);
	std::vector<VkImageView> getMousePickImageViews() const;

	VkExtent2D getMousePickExtent() const;

	void cleanup();

private:
	VkPipeline mousePickPipeline;
	uint32_t imageIndex;
	std::vector<VkImage> mousePickImages;
	std::vector<VkDeviceMemory> mousePickImageMemory;
	VulkanCore* engineCore;
	VkRenderPass mousePickRenderPass;
	std::vector<VkFramebuffer> mousePickFramebuffers;
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;

	void createMousePickImage();
	void createMousePickImageViews();
	void createDepthResources();
	void createMousePickRenderPass();
	void createMousePickFramebuffers();
	void createMousePickCommandBuffers();
	void createGraphicsPipeline();
};
