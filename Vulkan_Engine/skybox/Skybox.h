#pragma once
#include "vulkan/vulkan_core.h"

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"

// Vertex structure
struct SkyboxVertex {
	glm::vec3 pos;
};

struct SkyboxUniformBufferObject {
	glm::mat4 view;
	glm::mat4 proj;
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
	static void init(VkCommandPool commandPool, VkRenderPass renderPass);

	static void cleanup();

	static std::vector<VkDescriptorSet> getSkyboxDescriptorSet();

	static VkBuffer& getSkyboxVertexBuffer();

	static VkPipeline getSkyboxPipeline();

	static VkPipelineLayout getSkyboxPipelineLayout();

	static void updateSkyboxUniformBuffer(unsigned int currentImage, const glm::mat4& view, const glm::mat4& proj);

private:
	static VkImage skyboxImage;
	static VkImageView skyboxImageView;
	static VkDeviceMemory skyboxImageMemory;
	static VkSampler skyboxSampler;
	static std::vector<VkDescriptorSet> skyboxDescriptorSet;
	static VkDescriptorSetLayout skyboxDescriptorSetLayout;
	static VkDescriptorPool skyboxDescriptorPool;
	static std::string path;
	static VkBuffer skyboxVertexBuffer;
	static VkDeviceMemory skyboxVertexBufferMemory;
	static VkPipeline skyboxPipeline;
	static VkPipelineLayout skyboxPipelineLayout;
	static std::vector<VkBuffer> skyboxUniformBuffers;
	static std::vector<VkDeviceMemory> skyboxUniformBuffersMemory;

	static void createSkyboxUniformBuffers();

	static void createSkyboxPipeline(VkRenderPass renderPass);

	static void createSkyboxVertexBuffer(VkCommandPool commandPool);

	static void createDescriptorSet();

	static void createSkyboxSampler();

	static void createSkyboxImage(VkCommandPool commandPool, uint32_t width, uint32_t height);

	static void createSkyboxImageViews();

	static void loadSkyboxTextures(VkCommandPool commandPool);
};
