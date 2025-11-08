#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <Entity.h>
#include <vulkan/vulkan.h>
#include "SceneRenderer.h"
#include "core/utils/Utils.h"
#include "core/vulkancore.h"
#include "skybox/Skybox.h"

class ViewPort {
 public:
  VkExtent2D viewportExtent;
  std::vector<VkImageView> m_ViewportImageViews;
  std::vector<VkCommandBuffer> m_ViewportCommandBuffers;

  void init(VulkanCore* core, VkExtent2D viewportExtent);

  void createViewportImage();

  void createViewportImageViews();

  void recordViewportCommandBuffer(VkCommandBuffer commandBuffer,
                                   uint32_t imageIndex);

  void recreateViewport(VkExtent2D viewportExtent);

  void cleanup();

  void cleanupFramebuffers();

 private:
  std::vector<VkImage> m_ViewportImages;
  std::vector<VkDeviceMemory> m_DstImageMemory;
  VulkanCore* engineCore;
  VkRenderPass m_ViewportRenderPass;
  VkCommandPool m_ViewportCommandPool;
  std::vector<VkFramebuffer> m_ViewportFramebuffers;

  void createViewportRenderPass();

  void createViewportCommandBuffers();

  void createViewportFramebuffers();
};
