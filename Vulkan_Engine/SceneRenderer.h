#pragma once
#include <cstdint>

#include "vulkan/vulkan_core.h"

// Forward declarations
class ResourceContext; // Forward declaration
class VulkanCore;
class Entity;
class ViewPort;

namespace Renderer {
class RenderCommandList;
}

class SceneRenderer {
public:
	static void init(ResourceContext* resources);

	static void renderScene(Renderer::RenderCommandList& commandList,
							VkPipeline pipeline,
							VkPipelineLayout pipelineLayout,
							uint32_t currentFrame,
							uint64_t dynamicAlignment);

	static void renderEntity(const Entity* entity,
							 Renderer::RenderCommandList& commandList,
							 VkPipeline pipeline,
							 VkPipelineLayout pipelineLayout,
							 uint32_t currentFrame,
							 uint64_t dynamicAlignment,
							 int useMousePick);
	static void renderOutlineSelected(Renderer::RenderCommandList& commandList,
									  VkPipeline outlinePipeline,
									  VkPipelineLayout outlinePipelineLayout,
									  VkDescriptorSet outlineDescriptorSet);
	static void renderMousePick(Renderer::RenderCommandList& commandList,
								VkPipeline pipeline,
								VkPipelineLayout pipelineLayout,
								uint32_t currentFrame,
								uint64_t dynamicAlignment);

private:
	static ResourceContext* resources;
};
