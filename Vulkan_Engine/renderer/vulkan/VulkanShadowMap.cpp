#include "VulkanShadowMap.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <vector>

#include "core/utils/Utils.h"
#include "core/vulkancore.h" // MAX_FRAMES_IN_FLIGHT
#include "managers/MeshManager.h" // Added for Vertex::getBindingDescription/getAttributeDescriptions
#include "SceneRenderer.h"
#include "vulkan/vulkan_core.h"
#include "VulkanCommandList.h"
#include "VulkanDevice.h"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/geometric.hpp"
#include "glm/matrix.hpp"
#include "glm/trigonometric.hpp"

namespace Renderer {

VulkanShadowMap::~VulkanShadowMap() {
	cleanup();
}

void VulkanShadowMap::init(void* device, uint32_t shadowWidth, uint32_t shadowHeight) {
	vulkanDevice = static_cast<VulkanDevice*>(device);
	this->width = shadowWidth;
	this->height = shadowHeight;

	createRenderPass();
	createDepthResources();
	createFramebuffer();
	createSampler();
	createShadowDescriptorSetLayout();
	createShadowUBOs();
	createShadowDescriptorPool();
	createShadowDescriptorSets();
	createShadowPipelineLayout();
	createPipeline();

	shadowCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = VulkanCore::getCommandPool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t)shadowCommandBuffers.size();
	if (vkAllocateCommandBuffers(vulkanDevice->getDevice(), &allocInfo, shadowCommandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate shadow command buffers!");
	}
}

void VulkanShadowMap::cleanup() {
	if (!vulkanDevice)
		return;

	VkDevice device = vulkanDevice->getDevice();

	if (depthSampler != VK_NULL_HANDLE) {
		vkDestroySampler(device, depthSampler, nullptr);
		depthSampler = VK_NULL_HANDLE;
	}

	if (pipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(device, pipeline, nullptr);
		pipeline = VK_NULL_HANDLE;
	}

	if (shadowPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(device, shadowPipelineLayout, nullptr);
		shadowPipelineLayout = VK_NULL_HANDLE;
	}

	if (shadowDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, shadowDescriptorPool, nullptr);
		shadowDescriptorPool = VK_NULL_HANDLE;
	}

	if (shadowDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(device, shadowDescriptorSetLayout, nullptr);
		shadowDescriptorSetLayout = VK_NULL_HANDLE;
	}

	for (size_t i = 0; i < shadowUBOBuffers.size(); i++) {
		vkDestroyBuffer(device, shadowUBOBuffers[i], nullptr);
		vkFreeMemory(device, shadowUBOMemory[i], nullptr);
	}
	shadowUBOBuffers.clear();
	shadowUBOMemory.clear();
	shadowUBOMapped.clear();

	if (framebuffer != VK_NULL_HANDLE) {
		vkDestroyFramebuffer(device, framebuffer, nullptr);
		framebuffer = VK_NULL_HANDLE;
	}

	if (renderPass != VK_NULL_HANDLE) {
		vkDestroyRenderPass(device, renderPass, nullptr);
		renderPass = VK_NULL_HANDLE;
	}

	if (depthImageView != VK_NULL_HANDLE) {
		if (depthCubeImageView != VK_NULL_HANDLE) {
			vkDestroyImageView(device, depthCubeImageView, nullptr);
			depthCubeImageView = VK_NULL_HANDLE;
		}
		if (depth2DView != VK_NULL_HANDLE) {
			vkDestroyImageView(device, depth2DView, nullptr);
			depth2DView = VK_NULL_HANDLE;
		}
		vkDestroyImageView(device, depthImageView, nullptr);
		depthImageView = VK_NULL_HANDLE;
	}

	if (depthImage != VK_NULL_HANDLE) {
		vkDestroyImage(device, depthImage, nullptr);
		depthImage = VK_NULL_HANDLE;
	}

	if (depthImageMemory != VK_NULL_HANDLE) {
		vkFreeMemory(device, depthImageMemory, nullptr);
		depthImageMemory = VK_NULL_HANDLE;
	}
}

VkFormat VulkanShadowMap::findDepthFormat() {
	return vulkanDevice->findDepthFormat();
}

void VulkanShadowMap::createRenderPass() {
	VkAttachmentDescription attachmentDescription{};
	attachmentDescription.format = findDepthFormat();
	attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 0;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 0;
	subpass.pDepthStencilAttachment = &depthReference;

	// Use subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	// Enable Multiview for 6 Cubemap Faces
	const uint32_t viewMask = 0b00111111; // 63 (all 6 faces)
	const uint32_t correlationMask = 0b00111111;

	VkRenderPassMultiviewCreateInfo multiviewInfo{};
	multiviewInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO;
	multiviewInfo.subpassCount = 1;
	multiviewInfo.pViewMasks = &viewMask;
	multiviewInfo.dependencyCount = 0;
	multiviewInfo.pViewOffsets = nullptr;
	multiviewInfo.correlationMaskCount = 1;
	multiviewInfo.pCorrelationMasks = &correlationMask;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pNext = &multiviewInfo;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &attachmentDescription;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	if (vkCreateRenderPass(vulkanDevice->getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow map render pass!");
	}
}

void VulkanShadowMap::createDepthResources() {
	VkFormat depthFormat = findDepthFormat();

	uint32_t arrayLayers = 6;
	VkImageCreateFlags imageCreateFlags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	Utils::createImage(width,
					   height,
					   1,
					   VK_SAMPLE_COUNT_1_BIT,
					   depthFormat,
					   VK_IMAGE_TILING_OPTIMAL,
					   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					   depthImage,
					   depthImageMemory,
					   arrayLayers,
					   imageCreateFlags);

	// The framebuffer needs a 2D Array view of the layers to write to
	depthImageView = Utils::createImageView(
		depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, VK_IMAGE_VIEW_TYPE_2D_ARRAY, arrayLayers);

	// The Fragment Shader needs a Cube map view to sample from in 3D
	depthCubeImageView =
		Utils::createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, VK_IMAGE_VIEW_TYPE_CUBE, 6);

	// The Fragment Shader needs a 2D view for directional lights (binding 4)
	depth2DView =
		Utils::createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1, VK_IMAGE_VIEW_TYPE_2D, 1);
}

void VulkanShadowMap::createFramebuffer() {
	VkFramebufferCreateInfo framebufferInfo{};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = renderPass;
	framebufferInfo.attachmentCount = 1;
	framebufferInfo.pAttachments = &depthImageView;
	framebufferInfo.width = width;
	framebufferInfo.height = height;
	// The framebuffer must be set to 1 layer because Multiview implicitly broadcasts
	// across the layers defined by the viewMask in VkRenderPassMultiviewCreateInfo.
	framebufferInfo.layers = 1;

	if (vkCreateFramebuffer(vulkanDevice->getDevice(), &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow map framebuffer!");
	}
}

void VulkanShadowMap::createSampler() {
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_NEAREST;
	samplerInfo.minFilter = VK_FILTER_NEAREST;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	// We are manually comparing depth in the shader using texture().r
	// Enabling compareEnable requires sampler2DShadow and texture(..., vec3) in GLSL.
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

	if (vkCreateSampler(vulkanDevice->getDevice(), &samplerInfo, nullptr, &depthSampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow map shadow sampler!");
	}
}

VkPipelineLayout VulkanShadowMap::getPipelineLayout() const {
	return shadowPipelineLayout;
}

// ---------------------------------------------------------------------------
// New shadow-specific resource creation
// ---------------------------------------------------------------------------

void VulkanShadowMap::createShadowDescriptorSetLayout() {
	// binding = 0 : ShadowUBO (lightSpaceMatrix[6] + lightPos_farPlane)
	VkDescriptorSetLayoutBinding uboBinding{};
	uboBinding.binding = 0;
	uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboBinding.descriptorCount = 1;
	uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	uboBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &uboBinding;

	if (vkCreateDescriptorSetLayout(vulkanDevice->getDevice(), &layoutInfo, nullptr, &shadowDescriptorSetLayout) !=
		VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow descriptor set layout!");
	}
}

void VulkanShadowMap::createShadowPipelineLayout() {
	// push constant: mat4 model (64 bytes) used in vertex stage only
	VkPushConstantRange pushRange{};
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushRange.offset = 0;
	pushRange.size = sizeof(glm::mat4); // 64 bytes

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &shadowDescriptorSetLayout;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushRange;

	if (vkCreatePipelineLayout(vulkanDevice->getDevice(), &layoutInfo, nullptr, &shadowPipelineLayout) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow pipeline layout!");
	}
}

void VulkanShadowMap::createShadowUBOs() {
	shadowUBOBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	shadowUBOMemory.resize(MAX_FRAMES_IN_FLIGHT);
	shadowUBOMapped.resize(MAX_FRAMES_IN_FLIGHT);

	VkDeviceSize bufferSize = sizeof(ShadowUBO);

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		Utils::createBuffer(bufferSize,
							VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
							VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
							shadowUBOBuffers[i],
							shadowUBOMemory[i]);
		vkMapMemory(vulkanDevice->getDevice(), shadowUBOMemory[i], 0, bufferSize, 0, &shadowUBOMapped[i]);
	}
}

void VulkanShadowMap::createShadowDescriptorPool() {
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	if (vkCreateDescriptorPool(vulkanDevice->getDevice(), &poolInfo, nullptr, &shadowDescriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow descriptor pool!");
	}
}

void VulkanShadowMap::createShadowDescriptorSets() {
	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, shadowDescriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = shadowDescriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();

	shadowDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(vulkanDevice->getDevice(), &allocInfo, shadowDescriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate shadow descriptor sets!");
	}

	for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = shadowUBOBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(ShadowUBO);

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = shadowDescriptorSets[i];
		write.dstBinding = 0;
		write.dstArrayElement = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		write.descriptorCount = 1;
		write.pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(vulkanDevice->getDevice(), 1, &write, 0, nullptr);
	}
}

void VulkanShadowMap::updateShadowUBO(uint32_t frame,
									  const glm::mat4* lightSpaceMatrices,
									  const glm::vec4& lightPos_farPlane) {
	ShadowUBO ubo{};
	for (int i = 0; i < 6; i++)
		ubo.lightSpaceMatrix[i] = lightSpaceMatrices[i];
	ubo.lightPos_farPlane = lightPos_farPlane;
	memcpy(shadowUBOMapped[frame], &ubo, sizeof(ubo));
}

VkDescriptorSet VulkanShadowMap::getDescriptorSet(uint32_t frame) const {
	return shadowDescriptorSets[frame];
}

void VulkanShadowMap::createPipeline() {
	auto shadowVertShaderCode = Utils::readFile("shaders/shadow/shadow.vert.spv");
	VkShaderModule shadowVertShaderModule = createShaderModule(shadowVertShaderCode);

	auto shadowFragShaderCode = Utils::readFile("shaders/shadow/shadow.frag.spv");
	VkShaderModule shadowFragShaderModule = createShaderModule(shadowFragShaderCode);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = shadowVertShaderModule;
	vertShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = shadowFragShaderModule;
	fragShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	// Keep the full Vertex binding (stride covers the whole struct) so pos is
	// read at the correct byte offset, but only declare the one attribute the
	// shadow shader actually consumes (location 0 = position).
	auto bindingDescription = Vertex::getBindingDescription();

	VkVertexInputAttributeDescription posAttr{};
	posAttr.location = 0;
	posAttr.binding = 0;
	posAttr.format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
	posAttr.offset = offsetof(Vertex, pos);

	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.vertexAttributeDescriptionCount = 1;
	vertexInputInfo.pVertexAttributeDescriptions = &posAttr;

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
	rasterizer.depthBiasEnable = VK_TRUE;

	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.stencilTestEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 0;

	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_BIAS};

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
	pipelineInfo.layout = shadowPipelineLayout;
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(vulkanDevice->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) !=
		VK_SUCCESS) {
		throw std::runtime_error("failed to create shadow graphics pipeline!");
	}

	vkDestroyShaderModule(vulkanDevice->getDevice(), shadowVertShaderModule, nullptr);
	vkDestroyShaderModule(vulkanDevice->getDevice(), shadowFragShaderModule, nullptr);
}

void VulkanShadowMap::calculateShadowMatrices(const glm::vec4& positionType,
											  const glm::vec4& direction,
											  float far_plane,
											  float outerCutOff,
											  glm::mat4 outMatrices[6],
											  glm::vec4& outLightPosFarPlane) {
	glm::vec3 lPos(positionType.x, positionType.y, positionType.z);

	if (static_cast<int>(positionType.w) == 1) { // 1 = point light
		float pt_near = 0.1f;
		float pt_far = far_plane;
		glm::mat4 shadowProj = glm::perspective(glm::radians(90.0f), 1.0f, pt_near, pt_far);

		outMatrices[0] = shadowProj * glm::lookAt(lPos, lPos + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
		outMatrices[1] = shadowProj * glm::lookAt(lPos, lPos + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0));
		outMatrices[2] = shadowProj * glm::lookAt(lPos, lPos + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0));
		outMatrices[3] = shadowProj * glm::lookAt(lPos, lPos + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0));
		outMatrices[4] = shadowProj * glm::lookAt(lPos, lPos + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0));
		outMatrices[5] = shadowProj * glm::lookAt(lPos, lPos + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0));

		outLightPosFarPlane = glm::vec4(lPos, pt_far);
	} else if (static_cast<int>(positionType.w) == 2) { // 2 = spot light
		float spot_near = 0.1f;
		float spot_far = far_plane > 0.1f ? far_plane : 20.0f;
		float fov = glm::acos(outerCutOff) * 2.0f;
		glm::mat4 lightProjection = glm::perspective(fov, 1.0f, spot_near, spot_far);
		lightProjection[1][1] *= -1.0f;

		glm::vec3 lightDir = glm::normalize(glm::vec3(direction.x, direction.y, direction.z));
		glm::vec3 up = std::abs(lightDir.y) > 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);
		glm::mat4 lightView = glm::lookAt(lPos, lPos + lightDir, up);

		for (int i = 0; i < 6; i++) {
			outMatrices[i] = lightProjection * lightView;
		}
		outLightPosFarPlane = glm::vec4(lPos, 0.0f);
	} else { // 0 = directional light
		float shadowRadius = far_plane > 0.1f ? far_plane : 25.0f; // Use light's far plane range
		glm::vec3 cameraPos(positionType.x, positionType.y, positionType.z);

		glm::vec3 lightDir = glm::normalize(glm::vec3(direction.x, direction.y, direction.z));
		glm::vec3 up = std::abs(lightDir.y) > 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

		// Build a temporary light-space basis centered near the camera
		glm::mat4 lightView = glm::lookAt(cameraPos - lightDir * shadowRadius, cameraPos, up);

		// Snap origin to texel grid to prevent shadow swimming
		float texelsPerUnit = (float)width / (2.0f * shadowRadius);
		glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(texelsPerUnit));
		glm::mat4 lookAt = scaleMat * lightView;
		glm::mat4 lookAtInv = glm::inverse(lookAt);

		glm::vec3 origin(0.0f);
		origin = glm::vec3(lookAt * glm::vec4(origin, 1.0f));
		origin.x = std::floor(origin.x);
		origin.y = std::floor(origin.y);
		origin = glm::vec3(lookAtInv * glm::vec4(origin, 1.0f));

		glm::vec3 snappedEye = origin - lightDir * shadowRadius;
		glm::mat4 snappedView = glm::lookAt(snappedEye, origin, up);

		float near_plane = 0.1f;
		float f_plane = shadowRadius * 2.0f;
		glm::mat4 lightProjection =
			glm::orthoZO(-shadowRadius, shadowRadius, -shadowRadius, shadowRadius, near_plane, f_plane);

		for (int i = 0; i < 6; i++) {
			outMatrices[i] = lightProjection * snappedView;
		}
		outLightPosFarPlane = glm::vec4(0.0f);
	}
}

VkShaderModule VulkanShadowMap::createShaderModule(const std::vector<char>& code) {
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(vulkanDevice->getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module!");
	}
	return shaderModule;
}
void VulkanShadowMap::recordShadowCommandBuffer(uint32_t currentFrame,
												const glm::mat4* lightSpaceMatrices,
												const glm::vec4& lightPos_farPlane) {
	VkCommandBuffer shadowCmd = shadowCommandBuffers[currentFrame];
	vkResetCommandBuffer(shadowCmd, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	if (vkBeginCommandBuffer(shadowCmd, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording shadow command buffer!");
	}

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = framebuffer;
	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent.width = width;
	renderPassInfo.renderArea.extent.height = height;

	VkClearValue clearValue{};
	clearValue.depthStencil = {1.0f, 0};
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearValue;

	vkCmdBeginRenderPass(shadowCmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(width);
	viewport.height = static_cast<float>(height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(shadowCmd, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = {width, height};
	vkCmdSetScissor(shadowCmd, 0, 1, &scissor);

	// Depth bias to eliminate peter panning
	vkCmdSetDepthBias(shadowCmd, 1.25f, 0.0f, 1.75f);

	// Upload per-frame shadow light data
	updateShadowUBO(currentFrame, lightSpaceMatrices, lightPos_farPlane);

	// Draw each mesh entity using the extracted SceneRenderer encapsulation
	Renderer::VulkanCommandList commandList(shadowCmd);
	SceneRenderer::renderShadows(commandList, pipeline, shadowPipelineLayout, shadowDescriptorSets[currentFrame]);

	vkCmdEndRenderPass(shadowCmd);

	if (vkEndCommandBuffer(shadowCmd) != VK_SUCCESS) {
		throw std::runtime_error("failed to record shadow command buffer!");
	}
}

} // namespace Renderer
