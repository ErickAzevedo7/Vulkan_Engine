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

  static void RenderScene(
	  VkCommandBuffer commandBuffer,
	  VkPipeline pipeline,
	  VkPipelineLayout pipelineLayout,
	  uint32_t imageIndex);

  static void RenderEntity(const Entity* entity,
                           VkCommandBuffer commandBuffer,
                           VkPipeline pipeline,
                           VkPipelineLayout pipelineLayout, uint32_t imageIndex);

 private:
  static VulkanCore* engineCore;
};
