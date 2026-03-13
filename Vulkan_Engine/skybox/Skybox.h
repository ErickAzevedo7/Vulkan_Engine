#pragma once
#include "vulkan/vulkan_core.h"

namespace Renderer {
class VulkanDevice;
}

#include <glm/glm.hpp>
#include <vector>

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"

// Vertex structure
struct SkyboxVertex {
	glm::vec3 pos;
};


// Cube vertices (positions only)
const std::vector<SkyboxVertex> skyboxVertices = {
	// positions
	{{-1.0f, 1.0f, -1.0f}},	 {{-1.0f, -1.0f, -1.0f}}, {{1.0f, -1.0f, -1.0f}},
	{{1.0f, -1.0f, -1.0f}},	 {{1.0f, 1.0f, -1.0f}},	  {{-1.0f, 1.0f, -1.0f}},

	{{-1.0f, -1.0f, 1.0f}},	 {{-1.0f, -1.0f, -1.0f}}, {{-1.0f, 1.0f, -1.0f}},
	{{-1.0f, 1.0f, -1.0f}},	 {{-1.0f, 1.0f, 1.0f}},	  {{-1.0f, -1.0f, 1.0f}},

	{{1.0f, -1.0f, -1.0f}},	 {{1.0f, -1.0f, 1.0f}},	  {{1.0f, 1.0f, 1.0f}},
	{{1.0f, 1.0f, 1.0f}},	 {{1.0f, 1.0f, -1.0f}},	  {{1.0f, -1.0f, -1.0f}},

	{{-1.0f, -1.0f, 1.0f}},	 {{-1.0f, 1.0f, 1.0f}},	  {{1.0f, 1.0f, 1.0f}},
	{{1.0f, 1.0f, 1.0f}},	 {{1.0f, -1.0f, 1.0f}},	  {{-1.0f, -1.0f, 1.0f}},

	{{-1.0f, 1.0f, -1.0f}},	 {{1.0f, 1.0f, -1.0f}},	  {{1.0f, 1.0f, 1.0f}},
	{{1.0f, 1.0f, 1.0f}},	 {{-1.0f, 1.0f, 1.0f}},	  {{-1.0f, 1.0f, -1.0f}},

	{{-1.0f, -1.0f, -1.0f}}, {{-1.0f, -1.0f, 1.0f}},  {{1.0f, -1.0f, -1.0f}},
	{{1.0f, -1.0f, -1.0f}},	 {{-1.0f, -1.0f, 1.0f}},  {{1.0f, -1.0f, 1.0f}}};

class Skybox {
public:
	static void init(Renderer::VulkanDevice* device,
					 VkCommandPool commandPool,
					 VkRenderPass renderPass,
					 VkImageView envView,
					 VkSampler envSampler);

	static void cleanup();

	static std::vector<VkDescriptorSet> getSkyboxDescriptorSet();

	static VkBuffer& getSkyboxVertexBuffer();

	static VkPipeline getSkyboxPipeline();

	static VkPipelineLayout getSkyboxPipelineLayout();


private:
	static std::vector<VkDescriptorSet> skyboxDescriptorSet;
	static VkDescriptorSetLayout skyboxDescriptorSetLayout;
	static VkDescriptorPool skyboxDescriptorPool;
	static VkBuffer skyboxVertexBuffer;
	static VkDeviceMemory skyboxVertexBufferMemory;
	static Renderer::VulkanDevice* vulkanDevice;
	static VkPipeline skyboxPipeline;
	static VkPipelineLayout skyboxPipelineLayout;


	static void createSkyboxPipeline(VkRenderPass renderPass);

	static void createSkyboxVertexBuffer(VkCommandPool commandPool);

	static void createDescriptorSet(VkImageView envView, VkSampler envSampler);
};
