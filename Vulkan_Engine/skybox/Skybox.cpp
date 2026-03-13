#include "skybox.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stb_image.h>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/utils/Utils.h"
#include "core/vulkancore.h"
#include "renderer/vulkan/VulkanDevice.h"
#include "vulkan/vulkan_core.h"

#include "glm/ext/vector_float3.hpp"

// initialize static members
std::vector<VkDescriptorSet> Skybox::skyboxDescriptorSet;
VkDescriptorSetLayout Skybox::skyboxDescriptorSetLayout;
VkDescriptorPool Skybox::skyboxDescriptorPool;
VkBuffer Skybox::skyboxVertexBuffer;
VkDeviceMemory Skybox::skyboxVertexBufferMemory;
VkPipeline Skybox::skyboxPipeline;
VkPipelineLayout Skybox::skyboxPipelineLayout;
Renderer::VulkanDevice* Skybox::vulkanDevice = nullptr;

void Skybox::init(Renderer::VulkanDevice* device,
				  VkCommandPool commandPool,
				  VkRenderPass renderPass,
				  VkImageView envView,
				  VkSampler envSampler) {
	vulkanDevice = device;
	createDescriptorSet(envView, envSampler);
	createSkyboxVertexBuffer(commandPool);
	createSkyboxPipeline(renderPass);
}

void Skybox::cleanup() {
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
	}
	vkDestroyDescriptorPool(vulkanDevice->getDevice(), skyboxDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(vulkanDevice->getDevice(), skyboxDescriptorSetLayout, nullptr);
	vkDestroyBuffer(vulkanDevice->getDevice(), skyboxVertexBuffer, nullptr);
	vkFreeMemory(vulkanDevice->getDevice(), skyboxVertexBufferMemory, nullptr);
	vkDestroyPipeline(vulkanDevice->getDevice(), skyboxPipeline, nullptr);
	vkDestroyPipelineLayout(vulkanDevice->getDevice(), skyboxPipelineLayout, nullptr);
}

std::vector<VkDescriptorSet> Skybox::getSkyboxDescriptorSet() {
	return skyboxDescriptorSet;
}

VkBuffer& Skybox::getSkyboxVertexBuffer() {
	return skyboxVertexBuffer;
}

VkPipeline Skybox::getSkyboxPipeline() {
	return skyboxPipeline;
}

VkPipelineLayout Skybox::getSkyboxPipelineLayout() {
	return skyboxPipelineLayout;
}

void Skybox::createSkyboxPipeline(VkRenderPass renderPass) {
	auto vertShaderCode = Utils::readFile("shaders/skybox.vert.spv");
	auto fragShaderCode = Utils::readFile("shaders/skybox.frag.spv");

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

	// Vertex input (positions only)
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	// For skybox pipeline only:
	VkVertexInputBindingDescription bindingDescription{};
	bindingDescription.binding = 0;
	bindingDescription.stride = sizeof(glm::vec3); // or whatever your skybox vertex is
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription attributeDescription{};
	attributeDescription.binding = 0;
	attributeDescription.location = 0;
	attributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescription.offset = 0;

	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = 1;
	vertexInputInfo.pVertexAttributeDescriptions = &attributeDescription;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
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
	multisampling.sampleShadingEnable = VK_TRUE; // enable sample shading in the pipeline
	multisampling.minSampleShading = .2f; // min fraction for sample shading; closer to one is smoother
	multisampling.rasterizationSamples = static_cast<VkSampleCountFlagBits>(vulkanDevice->getMsaaSamples());

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_FALSE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(float) * 3 + sizeof(int);

	std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkDescriptorSetLayout layouts[] = {VulkanCore::getGlobalDescriptorSetLayout(), skyboxDescriptorSetLayout};

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 2;
	pipelineLayoutInfo.pSetLayouts = layouts;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(vulkanDevice->getDevice(), &pipelineLayoutInfo, nullptr, &skyboxPipelineLayout) !=
		VK_SUCCESS) {
		throw std::runtime_error("failed to create pipeline layout!");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
	pipelineInfo.basePipelineIndex = 0;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.layout = skyboxPipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(
			vulkanDevice->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &skyboxPipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(vulkanDevice->getDevice(), fragShaderModule, nullptr);
	vkDestroyShaderModule(vulkanDevice->getDevice(), vertShaderModule, nullptr);
}

void Skybox::createSkyboxVertexBuffer(VkCommandPool commandPool) {
	VkDeviceSize bufferSize = sizeof(skyboxVertices[0]) * skyboxVertices.size();
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	Utils::createBuffer(bufferSize,
						VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
						stagingBuffer,
						stagingBufferMemory);
	void* data;
	vkMapMemory(vulkanDevice->getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, skyboxVertices.data(), (size_t)bufferSize);
	vkUnmapMemory(vulkanDevice->getDevice(), stagingBufferMemory);
	Utils::createBuffer(bufferSize,
						VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
						skyboxVertexBuffer,
						skyboxVertexBufferMemory);
	Utils::copyBuffer(commandPool, stagingBuffer, skyboxVertexBuffer, bufferSize);
	vkDestroyBuffer(vulkanDevice->getDevice(), stagingBuffer, nullptr);
	vkFreeMemory(vulkanDevice->getDevice(), stagingBufferMemory, nullptr);
}

void Skybox::createDescriptorSet(VkImageView envView, VkSampler envSampler) {
	skyboxDescriptorSet.resize(MAX_FRAMES_IN_FLIGHT);

	VkDescriptorPoolSize poolSizes{};
	poolSizes.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSizes;
	poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	if (vkCreateDescriptorPool(vulkanDevice->getDevice(), &poolInfo, nullptr, &skyboxDescriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}

	std::array<VkDescriptorSetLayoutBinding, 2> samplerLayoutBinding{};
	samplerLayoutBinding[0].binding = 0;
	samplerLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	samplerLayoutBinding[0].descriptorCount = 1;
	samplerLayoutBinding[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	samplerLayoutBinding[0].pImmutableSamplers = nullptr;

	samplerLayoutBinding[1].binding = 1;
	samplerLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding[1].descriptorCount = 1;
	samplerLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding[1].pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &samplerLayoutBinding[1];

	if (vkCreateDescriptorSetLayout(vulkanDevice->getDevice(), &layoutInfo, nullptr, &skyboxDescriptorSetLayout) !=
		VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor set layout!");
	}

	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, skyboxDescriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = skyboxDescriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();

	if (vkAllocateDescriptorSets(vulkanDevice->getDevice(), &allocInfo, skyboxDescriptorSet.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = envView;
		imageInfo.sampler = envSampler;

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.pNext = nullptr;
		descriptorWrite.dstSet = skyboxDescriptorSet[i];
		descriptorWrite.dstBinding = 1;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(vulkanDevice->getDevice(), 1, &descriptorWrite, 0, nullptr);
	}
}
