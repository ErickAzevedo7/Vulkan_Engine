#pragma once
#include <cstdint>

#include "vulkan/vulkan_core.h"

// Forward declarations
class ResourceContext; // Forward declaration
class VulkanCore;
class Entity;
class ViewPort;

class SceneRenderer {
public:
	static void init(VulkanCore* engineCore, ResourceContext* resources);

	static void renderScene(VkCommandBuffer commandBuffer,
							VkPipeline pipeline,
							VkPipelineLayout pipelineLayout,
							uint32_t currentFrame,
							uint64_t dynamicAlignment);

	static void renderEntity(const Entity* entity,
							 VkCommandBuffer commandBuffer,
							 VkPipeline pipeline,
							 VkPipelineLayout pipelineLayout,
							 uint32_t currentFrame,
							 uint64_t dynamicAlignment,
							 int useMousePick);
	static void renderOutlineSelected(VkCommandBuffer commandBuffer,
									  VkPipeline outlinePipeline,
									  VkPipelineLayout outlinePipelineLayout,
									  VkDescriptorSet outlineDescriptorSet);
	static void renderMousePick(VkCommandBuffer commandBuffer,
								VkPipeline pipeline,
								VkPipelineLayout pipelineLayout,
								uint32_t currentFrame,
								uint64_t dynamicAlignment);

private:
	static VulkanCore* engineCore;
	static ResourceContext* resources;
};
