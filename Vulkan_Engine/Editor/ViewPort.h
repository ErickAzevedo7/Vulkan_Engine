#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

#include "vulkan/vulkan_core.h"

#include "glm/ext/matrix_float4x4.hpp"


namespace Renderer {
class VulkanDevice;
}

class ViewPort {
public:
	VkExtent2D viewportExtent;
	std::vector<VkImageView> m_ViewportImageViews;
	std::vector<VkCommandBuffer> m_ViewportCommandBuffers;

	void init(Renderer::VulkanDevice* device, VkExtent2D viewportExtent);

	void createViewportImage();

	void createViewportImageViews();

	void
	recordViewportCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex, const glm::mat4& lightSpaceMatrix);

	void recreateViewport(VkExtent2D viewportExtent);

	void cleanup();
	void createColorResources();
	void createDepthResources();

	void cleanupFramebuffers();

private:
	std::vector<VkImage> m_ViewportImages;
	std::vector<VkDeviceMemory> m_DstImageMemory;
	Renderer::VulkanDevice* vulkanDevice;
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
