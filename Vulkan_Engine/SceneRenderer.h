#pragma once
#include "Entity.h"
#include "core/vulkancore.h"
#include <stdexcept>
#include "managers/SceneManager.h"

#include "components/MeshComponent.h"
#include "components/Transform.h"

class SceneRenderer {
 public:
  static void init(VulkanCore* engineCore);

  static void renderScene(
	  VkCommandBuffer commandBuffer,
	  VkPipeline pipeline,
	  VkPipelineLayout pipelineLayout,
	  uint32_t imageIndex);

  static void renderEntity(const Entity* entity,
                           VkCommandBuffer commandBuffer,
                           VkPipeline pipeline,
                           VkPipelineLayout pipelineLayout, uint32_t imageIndex, int useMousePick);
  static void renderMousePick(VkCommandBuffer commandBuffer, VkPipeline pipeline,
                       VkPipelineLayout pipelineLayout, uint32_t imageIndex);

private:
  static VulkanCore* engineCore;
};
