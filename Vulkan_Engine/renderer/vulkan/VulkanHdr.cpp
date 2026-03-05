#include "VulkanHdr.h"

#include <array>
#include <cstdint>
#include <stdexcept>

#include "core/utils/Utils.h"
#include "core/vulkancore.h"
#include "renderer/vulkan/VulkanDevice.h"

namespace Renderer {

void VulkanHdr::init(void* device, void* hdrResolveImageView, uint32_t width, uint32_t height) {
	this->vulkanDevice = static_cast<Renderer::VulkanDevice*>(device);
	this->hdrResolveImageView = static_cast<VkImageView>(hdrResolveImageView);
	this->viewportExtent = {width, height};

	createHdrRenderPass();
	createLdrImages();
	createHdrFramebuffers();
	createHdrSampler();
	createDescriptorPool();
	createHdrDescriptorSets();
	createGraphicsPipeline();
	createCommandBuffers();
}

void VulkanHdr::recordHdrCommandBuffer(uint32_t currentFrame, uint32_t imageIndex, float exposure) {
	VkCommandBuffer commandBuffer = hdrCommandBuffers[currentFrame];

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording HDR command buffer!");
	}

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = hdrRenderPass;
	renderPassInfo.framebuffer = hdrFramebuffers[imageIndex];
	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = viewportExtent;

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)viewportExtent.width;
	viewport.height = (float)viewportExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = viewportExtent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hdrPipeline);

	vkCmdBindDescriptorSets(commandBuffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							hdrPipelineLayout,
							0,
							1,
							&hdrDescriptorSets[currentFrame],
							0,
							nullptr);

	vkCmdPushConstants(commandBuffer, hdrPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &exposure);

	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record HDR command buffer!");
	}
}

void VulkanHdr::recreateHdr(void* hdrResolveImageView, uint32_t width, uint32_t height) {
	if (width == 0 || height == 0)
		return;

	cleanupFramebuffers();

	this->hdrResolveImageView = static_cast<VkImageView>(hdrResolveImageView);
	this->viewportExtent = {width, height};

	createLdrImages();
	createHdrFramebuffers();

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = this->hdrResolveImageView;
		imageInfo.sampler = hdrSampler;

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = hdrDescriptorSets[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(vulkanDevice->getDevice(), 1, &descriptorWrite, 0, nullptr);
	}
}

void VulkanHdr::cleanupFramebuffers() {
	for (auto framebuffer : hdrFramebuffers) {
		vkDestroyFramebuffer(vulkanDevice->getDevice(), framebuffer, nullptr);
	}

	for (auto imageView : ldrImageViews) {
		vkDestroyImageView(vulkanDevice->getDevice(), imageView, nullptr);
	}

	for (auto image : ldrImages) {
		vkDestroyImage(vulkanDevice->getDevice(), image, nullptr);
	}

	for (auto memory : ldrImageMemory) {
		vkFreeMemory(vulkanDevice->getDevice(), memory, nullptr);
	}

	ldrImageViews.clear();
	ldrImages.clear();
	ldrImageMemory.clear();
}

void VulkanHdr::cleanup() {
	if (!vulkanDevice)
		return;

	cleanupFramebuffers();
	vkDestroyPipeline(vulkanDevice->getDevice(), hdrPipeline, nullptr);
	vkDestroyPipelineLayout(vulkanDevice->getDevice(), hdrPipelineLayout, nullptr);
	vkDestroyRenderPass(vulkanDevice->getDevice(), hdrRenderPass, nullptr);
	vkDestroyDescriptorSetLayout(vulkanDevice->getDevice(), hdrDescriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(vulkanDevice->getDevice(), hdrDescriptorPool, nullptr);
	vkDestroySampler(vulkanDevice->getDevice(), hdrSampler, nullptr);

	if (!hdrCommandBuffers.empty()) {
		vkFreeCommandBuffers(vulkanDevice->getDevice(),
							 VulkanCore::getCommandPool(),
							 static_cast<uint32_t>(hdrCommandBuffers.size()),
							 hdrCommandBuffers.data());
		hdrCommandBuffers.clear();
	}
}

void VulkanHdr::createHdrRenderPass() {
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = VK_FORMAT_B8G8R8A8_SRGB; // Outputting to LDR format for display
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // To be read by outline pass/ImGui

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(vulkanDevice->getDevice(), &renderPassInfo, nullptr, &hdrRenderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create HDR render pass!");
	}
}

void VulkanHdr::createLdrImages() {
	uint32_t imageCount = vulkanDevice->getSwapChainImageCount();
	ldrImages.resize(imageCount);
	ldrImageMemory.resize(imageCount);
	ldrImageViews.resize(imageCount);

	for (uint32_t i = 0; i < imageCount; i++) {
		VkImageCreateInfo imageCreateCI{};
		imageCreateCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateCI.imageType = VK_IMAGE_TYPE_2D;
		imageCreateCI.format = VK_FORMAT_B8G8R8A8_SRGB;
		imageCreateCI.extent.width = viewportExtent.width;
		imageCreateCI.extent.height = viewportExtent.height;
		imageCreateCI.extent.depth = 1;
		imageCreateCI.arrayLayers = 1;
		imageCreateCI.mipLevels = 1;
		imageCreateCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		vkCreateImage(vulkanDevice->getDevice(), &imageCreateCI, nullptr, &ldrImages[i]);

		VkMemoryRequirements memRequirements;
		VkMemoryAllocateInfo memAllocInfo{};
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		vkGetImageMemoryRequirements(vulkanDevice->getDevice(), ldrImages[i], &memRequirements);
		memAllocInfo.allocationSize = memRequirements.size;
		memAllocInfo.memoryTypeIndex =
			Utils::findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(vulkanDevice->getDevice(), &memAllocInfo, nullptr, &ldrImageMemory[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}
		vkBindImageMemory(vulkanDevice->getDevice(), ldrImages[i], ldrImageMemory[i], 0);

		ldrImageViews[i] = Utils::createImageView(ldrImages[i], VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}
}

void VulkanHdr::createHdrFramebuffers() {
	hdrFramebuffers.resize(ldrImageViews.size());
	for (size_t i = 0; i < ldrImageViews.size(); i++) {
		VkImageView attachment = ldrImageViews[i];

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = hdrRenderPass;
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = &attachment;
		framebufferInfo.width = viewportExtent.width;
		framebufferInfo.height = viewportExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(vulkanDevice->getDevice(), &framebufferInfo, nullptr, &hdrFramebuffers[i]) !=
			VK_SUCCESS) {
			throw std::runtime_error("failed to create HDR framebuffer!");
		}
	}
}

void VulkanHdr::createHdrSampler() {
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 1.0f;

	if (vkCreateSampler(vulkanDevice->getDevice(), &samplerInfo, nullptr, &hdrSampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create HDR sampler!");
	}
}

void VulkanHdr::createDescriptorPool() {
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);

	if (vkCreateDescriptorPool(vulkanDevice->getDevice(), &poolInfo, nullptr, &hdrDescriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create HDR descriptor pool!");
	}
}

void VulkanHdr::createHdrDescriptorSets() {
	VkDescriptorSetLayoutBinding samplerLayoutBinding{};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &samplerLayoutBinding;

	if (vkCreateDescriptorSetLayout(vulkanDevice->getDevice(), &layoutInfo, nullptr, &hdrDescriptorSetLayout) !=
		VK_SUCCESS) {
		throw std::runtime_error("failed to create HDR descriptor set layout!");
	}

	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, hdrDescriptorSetLayout);
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = hdrDescriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();

	hdrDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	if (vkAllocateDescriptorSets(vulkanDevice->getDevice(), &allocInfo, hdrDescriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate HDR descriptor sets!");
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = hdrResolveImageView;
		imageInfo.sampler = hdrSampler;

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = hdrDescriptorSets[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(vulkanDevice->getDevice(), 1, &descriptorWrite, 0, nullptr);
	}
}

void VulkanHdr::createGraphicsPipeline() {
	auto vertShaderCode = Utils::readFile("shaders/hdr/postprocess_hdr_vert.spv");
	auto fragShaderCode = Utils::readFile("shaders/hdr/postprocess_hdr_frag.spv");

	VkShaderModule vertShaderModule, fragShaderModule;
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

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;

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
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;

	std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(float);

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &hdrDescriptorSetLayout;
	pipelineLayoutInfo.pushConstantRangeCount = 1;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(vulkanDevice->getDevice(), &pipelineLayoutInfo, nullptr, &hdrPipelineLayout) !=
		VK_SUCCESS) {
		throw std::runtime_error("failed to create HDR pipeline layout!");
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.layout = hdrPipelineLayout;
	pipelineInfo.renderPass = hdrRenderPass;
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(vulkanDevice->getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &hdrPipeline) !=
		VK_SUCCESS) {
		throw std::runtime_error("failed to create HDR graphics pipeline!");
	}

	vkDestroyShaderModule(vulkanDevice->getDevice(), fragShaderModule, nullptr);
	vkDestroyShaderModule(vulkanDevice->getDevice(), vertShaderModule, nullptr);
}

void VulkanHdr::createCommandBuffers() {
	hdrCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = VulkanCore::getCommandPool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(hdrCommandBuffers.size());

	if (vkAllocateCommandBuffers(VulkanCore::getDevice(), &allocInfo, hdrCommandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate HDR command buffers!");
	}
}

} // namespace Renderer
