#pragma once

#include "core/vulkancore.h"
#include "vulkan/vulkan_core.h"

#include <cstdint>
#include <vector>

class ViewPort {
public:
	VkExtent2D viewportExtent;
	std::vector<VkImageView> m_ViewportImageViews;
	std::vector<VkCommandBuffer> m_ViewportCommandBuffers;

	void init(VulkanCore* core, VkExtent2D viewportExtent);

	void createViewportImage();

	void createViewportImageViews();

	void recordViewportCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

	void recreateViewport(VkExtent2D viewportExtent);

	void cleanup();
	void createColorResources();
	void createDepthResources();

	void cleanupFramebuffers();

private:
	std::vector<VkImage> m_ViewportImages;
	std::vector<VkDeviceMemory> m_DstImageMemory;
	VulkanCore* engineCore;
	VkRenderPass m_ViewportRenderPass;
	VkCommandPool m_ViewportCommandPool;
	std::vector<VkFramebuffer> m_ViewportFramebuffers;
	VkImage colorImage;
	VkDeviceMemory colorImageMemory;
	VkImageView colorImageView;
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;

	void createViewportRenderPass();

	void createViewportCommandBuffers();

	void createViewportFramebuffers();
};
