#pragma once
#include <cstdint>
#include <vector>

#include "vulkan/vulkan_core.h"
#define MAX_OBJECTS 1000

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
	static void initDescriptorResources(VkDevice device, VkDescriptorPool pool, VkPhysicalDevice physicalDevice);
	static void cleanup();

	static void renderScene(Renderer::RenderCommandList& commandList,
							VkPipeline pipeline,
							VkPipelineLayout pipelineLayout,
							uint32_t currentFrame,
							VkDescriptorSet globalSet);

	static void renderShadows(Renderer::RenderCommandList& commandList,
							  VkPipeline shadowPipeline,
							  VkPipelineLayout shadowPipelineLayout,
							  VkDescriptorSet shadowDescriptorSet);

	static void renderEntity(const Entity* entity,
							 Renderer::RenderCommandList& commandList,
							 VkPipeline pipeline,
							 VkPipelineLayout pipelineLayout,
							 uint32_t currentFrame,
							 int useMousePick);
	static void renderOutlineSelected(Renderer::RenderCommandList& commandList,
									  VkPipeline outlinePipeline,
									  VkPipelineLayout outlinePipelineLayout,
									  VkDescriptorSet outlineDescriptorSet);
	static void renderMousePick(Renderer::RenderCommandList& commandList,
								VkPipeline pipeline,
								VkPipelineLayout pipelineLayout,
								uint32_t currentFrame,
								VkDescriptorSet globalSet);

	static std::vector<VkDescriptorSet>& getPerObjectDescriptorSets() {
		return perObjectDescriptorSets;
	}
	static VkDeviceSize getDynamicAlignment() {
		return dynamicAlignment;
	}
	static VkDescriptorSetLayout getPerObjectDescriptorSetLayout() {
		return perObjectDescriptorSetLayout;
	}
	static std::vector<void*>& getUniformBuffersMapped() {
		return uniformBuffersMapped;
	}

private:
	static ResourceContext* resources;

	static VkDevice device;
	static VkDescriptorPool descriptorPool;

	// Set 3: Per-Object Data
	static VkDescriptorSetLayout perObjectDescriptorSetLayout;
	static std::vector<VkDescriptorSet> perObjectDescriptorSets;
	static std::vector<VkBuffer> uniformBuffers;
	static std::vector<VkDeviceMemory> uniformBuffersMemory;
	static std::vector<void*> uniformBuffersMapped;
	static VkDeviceSize dynamicAlignment;

	static void createPerObjectDescriptorSetLayout();
	static void createUniformBuffers(VkPhysicalDevice physicalDevice);
	static void createPerObjectDescriptorSets();
};
