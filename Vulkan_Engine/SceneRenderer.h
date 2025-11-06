#pragma once
#include <stdexcept>
#include "Editor/ViewPort.h"
#include "Entity.h"
#include "components/MeshComponent.h"
#include "components/Transform.h"
#include "core/vulkancore.h"
#include "managers/SceneManager.h"

class SceneRenderer {
 public:
  static void init(VulkanCore* engineCore);

  static void renderScene(VkCommandBuffer commandBuffer,
                          VkPipeline pipeline,
                          VkPipelineLayout pipelineLayout,
                          uint32_t imageIndex);

  static void renderEntity(const Entity* entity,
                           VkCommandBuffer commandBuffer,
                           VkPipeline pipeline,
                           VkPipelineLayout pipelineLayout,
                           uint32_t imageIndex,
                           int useMousePick);
  static void renderOutlineSelected(VkCommandBuffer commandBuffer,
                                    VkPipeline outlinePipeline,
                                    VkPipelineLayout outlinePipelineLayout, VkDescriptorSet outlineDescriptorSet);
  static void renderMousePick(VkCommandBuffer commandBuffer,
                              VkPipeline pipeline,
                              VkPipelineLayout pipelineLayout,
                              uint32_t imageIndex);

 private:
  static VulkanCore* engineCore;
};
