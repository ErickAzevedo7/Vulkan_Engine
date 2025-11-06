#pragma once
#include "SceneRenderer.h"
#include "core/vulkancore.h"

class Outline {
 public:
  std::vector<VkImageView> outlineColorImageViews;
  std::vector<VkCommandBuffer> outlineCommandBuffers;

  void init(VulkanCore* core, std::vector<VkImageView> IDimageViews, std::vector<VkImageView> outlineColorImageViews, VkExtent2D
            viewportExtent);
  void recordOutlineCommandBuffer(VkCommandBuffer commandBuffer,
                                    uint32_t imageIndex);
  void cleanupFramebuffers();

  void recreateOutline(std::vector<VkImageView> IDimageViews, std::vector<VkImageView> outlineColorImageViews, VkExtent2D viewportExtent);

 private:
  VkPipeline outlinePipeline;
  VkPipelineLayout outlinePipelineLayout;
  uint32_t imageIndex;
  VulkanCore* engineCore;
  VkRenderPass outlineRenderPass;
  std::vector<VkFramebuffer> outlineFramebuffers;
  VkDescriptorSetLayout outlineDescriptorSetLayout;
  std::vector<VkDescriptorSet> outlineDescriptorSets;
  VkSampler outlineSampler;
  std::vector<VkImageView> IDimageViews;
  VkDescriptorPool outlineDescriptorPool;
  VkExtent2D viewportExtent;
  

  void cleanup();
  void createOutlineRenderPass();
  void createOutlineFramebuffers();
  void createGraphicsPipeline();
  void createOutlineDescriptorSets();
  void createOutlineSampler();
  void createDescriptorPool();
};
