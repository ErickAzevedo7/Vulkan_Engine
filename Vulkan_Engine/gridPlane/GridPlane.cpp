#include "GridPlane.h"

#include "core/utils/Utils.h"
#include "core/vulkancore.h"
#include "managers/MeshManager.h"
#include "vulkan/vulkan_core.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/matrix.hpp"

// initialize static members
VkPipeline GridPlane::gridPLanePipeline;
VkPipelineLayout GridPlane::gridPlanePipelineLayout;
VkDescriptorPool GridPlane::gridPlaneDescriptorPool;
VkDescriptorSetLayout GridPlane::gridPlaneDescriptorSetLayout;
std::vector<VkDescriptorSet> GridPlane::gridPlaneDescriptorSets;
std::vector<VkBuffer> GridPlane::gridPlaneUniformBuffers;
std::vector<VkDeviceMemory> GridPlane::gridPlaneUniformBuffersMemory;
std::vector<VkBuffer> GridPlane::gridParamsUniformBuffers;
std::vector<VkDeviceMemory> GridPlane::gridParamsUniformBuffersMemory;

void GridPlane::init(VkCommandPool commandPool, VkRenderPass renderPass) {
	createGridPlaneUniformBuffers();
	createDescriptorSets();
	createGridPlanePipeline(renderPass);
}

void GridPlane::cleanup() {
	vkDestroyPipeline(VulkanCore::getDevice(), gridPLanePipeline, nullptr);
	vkDestroyPipelineLayout(VulkanCore::getDevice(), gridPlanePipelineLayout, nullptr);
	vkDestroyDescriptorPool(VulkanCore::getDevice(), gridPlaneDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(VulkanCore::getDevice(), gridPlaneDescriptorSetLayout, nullptr);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		vkDestroyBuffer(VulkanCore::getDevice(), gridPlaneUniformBuffers[i], nullptr);
		vkFreeMemory(VulkanCore::getDevice(), gridPlaneUniformBuffersMemory[i], nullptr);
		vkDestroyBuffer(VulkanCore::getDevice(), gridParamsUniformBuffers[i], nullptr);
		vkFreeMemory(VulkanCore::getDevice(), gridParamsUniformBuffersMemory[i], nullptr);
	}
}

VkPipeline GridPlane::getGridPlanePipeline() {
	return gridPLanePipeline;
}

VkPipelineLayout GridPlane::getGridPlanePipelineLayout() {
	return gridPlanePipelineLayout;
}

std::vector<VkDescriptorSet> GridPlane::getGridPlaneDescriptorSets() {
	return gridPlaneDescriptorSets;
}

void GridPlane::updateUniformBuffer(uint32_t currentImage,
									const glm::mat4& model,
									const glm::mat4& view,
									const glm::mat4& proj) {
	UniformBufferObject ubo{};
	ubo.model = model;
	ubo.normal = glm::transpose(glm::inverse(model));
	ubo.view = view;
	ubo.proj = proj;

	void* data;
	vkMapMemory(VulkanCore::getDevice(), gridPlaneUniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(VulkanCore::getDevice(), gridPlaneUniformBuffersMemory[currentImage]);
}

void GridPlane::updateGridParamsBuffer(unsigned int currentImage,
									   const glm::vec3& cameraWorldPos,
									   float gridSize,
									   float minPixelsBetweenCells,
									   float gridCellSize,
									   const glm::vec4& gridColorThin,
									   const glm::vec4& gridColorThick) {
	GridParamsUBO ubo{};
	ubo.gCameraWorldPos = cameraWorldPos;
	ubo.gGridSize = gridSize;
	ubo.gGridMinPixelsBetweenCells = minPixelsBetweenCells;
	ubo.gGridCellSize = gridCellSize;
	ubo.gGridColorThin = gridColorThin;
	ubo.gGridColorThick = gridColorThick;
	void* data;
	vkMapMemory(VulkanCore::getDevice(), gridParamsUniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(VulkanCore::getDevice(), gridParamsUniformBuffersMemory[currentImage]);
}

void GridPlane::createGridPlanePipeline(VkRenderPass renderPass) {
	auto vertShaderCode = Utils::readFile("shaders/gridplane.vert.spv");
	auto fragShaderCode = Utils::readFile("shaders/gridplane.frag.spv");

	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;

	Utils::createShaderModule(vertShaderCode, vertShaderModule);
	Utils::createShaderModule(fragShaderCode, fragShaderModule);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	auto bindingDescription = Vertex::getBindingDescription();

	std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(Vertex, pos);

	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Vertex, normal);

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_NONE;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VulkanCore::getmsaaSamples();

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &gridPlaneDescriptorSetLayout;

	if (vkCreatePipelineLayout(VulkanCore::getDevice(), &pipelineLayoutInfo, nullptr, &gridPlanePipelineLayout) !=
		VK_SUCCESS) {
		throw std::runtime_error("failed to create grid pipeline layout!");
	}

	std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = gridPlanePipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(
			VulkanCore::getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &gridPLanePipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create grid graphics pipeline!");
	}

	vkDestroyShaderModule(VulkanCore::getDevice(), vertShaderModule, nullptr);
	vkDestroyShaderModule(VulkanCore::getDevice(), fragShaderModule, nullptr);
}

void GridPlane::createDescriptorSets() {
	gridPlaneDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

	VkDescriptorPoolSize poolSizes{};
	poolSizes.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSizes;
	poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	if (vkCreateDescriptorPool(VulkanCore::getDevice(), &poolInfo, nullptr, &gridPlaneDescriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}

	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding gridParamsLayoutBinding{};
	gridParamsLayoutBinding.binding = 1;
	gridParamsLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	gridParamsLayoutBinding.descriptorCount = 1;
	gridParamsLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	gridParamsLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 2> bindings = {samplerLayoutBinding, gridParamsLayoutBinding};

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(VulkanCore::getDevice(), &layoutInfo, nullptr, &gridPlaneDescriptorSetLayout) !=
		VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, gridPlaneDescriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = gridPlaneDescriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();

	if (vkAllocateDescriptorSets(VulkanCore::getDevice(), &allocInfo, gridPlaneDescriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = gridPlaneUniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorBufferInfo gridParamsBufferInfo{};
		gridParamsBufferInfo.buffer = gridParamsUniformBuffers[i];
		gridParamsBufferInfo.offset = 0;
		gridParamsBufferInfo.range = sizeof(GridParamsUBO);

		std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = gridPlaneDescriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = gridPlaneDescriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pBufferInfo = &gridParamsBufferInfo;

		vkUpdateDescriptorSets(VulkanCore::getDevice(),
							   static_cast<uint32_t>(descriptorWrites.size()),
							   descriptorWrites.data(),
							   0,
							   nullptr);
	}
}

void GridPlane::createGridPlaneUniformBuffers() {
	VkDeviceSize bufferSize = sizeof(UniformBufferObject);
	VkDeviceSize gridBufferSize = sizeof(GridParamsUBO);
	gridPlaneUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	gridPlaneUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
	gridParamsUniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	gridParamsUniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		Utils::createBuffer(bufferSize,
							VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
							VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
							gridPlaneUniformBuffers[i],
							gridPlaneUniformBuffersMemory[i]);

		Utils::createBuffer(gridBufferSize,
							VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
							VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
							gridParamsUniformBuffers[i],
							gridParamsUniformBuffersMemory[i]);
	}
}
