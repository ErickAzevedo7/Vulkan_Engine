#pragma once
#include "core/vulkancore.h"
#include "SceneRenderer.h"

class MousePick {
 public:
  VkExtent2D mousePickExtent;
  std::vector<VkImageView> mousePickImageViews;
  std::vector<VkCommandBuffer> mousePickCommandBuffers;

  void init(VulkanCore* core);
  void recordMousePickCommandBuffer(VkCommandBuffer commandBuffer,
                                   uint32_t imageIndex, VkExtent2D viewportExtent);
  void cleanupFramebuffers();
  void recreateMousePick();

  uint32_t getEntityIDAt(int x, int y);
  std::vector<VkImageView> getMousePickImageViews() const;

  VkExtent2D getMousePickExtent() const;

private:
  VkPipeline mousePickPipeline;
  uint32_t imageIndex;
  std::vector<VkImage> mousePickImages;
  std::vector<VkDeviceMemory> mousePickImageMemory;
  VulkanCore* engineCore;
  VkRenderPass mousePickRenderPass;
  std::vector<VkFramebuffer> mousePickFramebuffers;
  
	
  
  void cleanup();
  void createMousePickImage();
  void createMousePickImageViews();
  void createMousePickRenderPass();
  void createMousePickFramebuffers();
  void createMousePickCommandBuffers();
  void createGraphicsPipeline();
};
