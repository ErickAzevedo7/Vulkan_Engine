#pragma once
#include "vulkan/vulkan_core.h"

#include <vector>

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"

struct GridParamsUBO {
	alignas(16) glm::vec3 gCameraWorldPos;
	alignas(4) float gGridSize;
	alignas(4) float gGridMinPixelsBetweenCells;
	alignas(4) float gGridCellSize;
	alignas(16) glm::vec4 gGridColorThin;
	alignas(16) glm::vec4 gGridColorThick;
};

class GridPlane {
public:
	static void init(VkCommandPool commandPool, VkRenderPass renderPass);

	static void cleanup();

	static VkPipeline getGridPlanePipeline();

	static VkPipelineLayout getGridPlanePipelineLayout();

	static std::vector<VkDescriptorSet> getGridPlaneDescriptorSets();

	static void updateUniformBuffer(unsigned int currentImage,
									const glm::mat4& model,
									const glm::mat4& view,
									const glm::mat4& proj);
	static void updateGridParamsBuffer(unsigned int currentImage,
									   const glm::vec3& cameraWorldPos,
									   float gridSize,
									   float minPixelsBetweenCells,
									   float gridCellSize,
									   const glm::vec4& gridColorThin,
									   const glm::vec4& gridColorThick);

private:
	static VkPipeline gridPLanePipeline;
	static VkPipelineLayout gridPlanePipelineLayout;
	static VkDescriptorPool gridPlaneDescriptorPool;
	static VkDescriptorSetLayout gridPlaneDescriptorSetLayout;
	static std::vector<VkDescriptorSet> gridPlaneDescriptorSets;
	static std::vector<VkBuffer> gridPlaneUniformBuffers;
	static std::vector<VkDeviceMemory> gridPlaneUniformBuffersMemory;
	static std::vector<VkBuffer> gridParamsUniformBuffers;
	static std::vector<VkDeviceMemory> gridParamsUniformBuffersMemory;

	static void createGridPlanePipeline(VkRenderPass renderPass);

	static void createDescriptorSets();

	static void createGridPlaneUniformBuffers();
};
