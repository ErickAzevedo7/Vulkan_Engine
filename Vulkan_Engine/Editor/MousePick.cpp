#include "MousePick.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "core/utils/Utils.h"
#include "managers/MeshManager.h"
#include "SceneRenderer.h"
#include "vulkan/vulkan_core.h"

void MousePick::init(VulkanCore* core, ResourceContext* resources) {
	engineCore = core;
	mousePickExtent = engineCore->getSwapChainExtent();

	createMousePickImage();

	createMousePickImageViews();

	createDepthResources();

	mousePickCommandBuffers.resize(mousePickImageViews.size());

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = VulkanCore::getCommandPool();
	allocInfo.commandBufferCount = static_cast<uint32_t>(mousePickCommandBuffers.size());

	if (vkAllocateCommandBuffers(VulkanCore::getDevice(), &allocInfo, mousePickCommandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	}

	// MeshManager defaults are now loaded in main.cpp via ResourceContext

	createMousePickRenderPass();

	createMousePickFramebuffers();

	createGraphicsPipeline();
}

void MousePick::cleanupFramebuffers() {
	for (auto framebuffer : mousePickFramebuffers) {
		vkDestroyFramebuffer(VulkanCore::getDevice(), framebuffer, nullptr);
	}

	for (auto imageView : mousePickImageViews) {
		vkDestroyImageView(VulkanCore::getDevice(), imageView, nullptr);
	}

	for (auto image : mousePickImages) {
		vkDestroyImage(VulkanCore::getDevice(), image, nullptr);
	}

	for (auto memory : mousePickImageMemory) {
		vkFreeMemory(VulkanCore::getDevice(), memory, nullptr);
	}

	vkDestroyImageView(VulkanCore::getDevice(), depthImageView, nullptr);
	vkDestroyImage(VulkanCore::getDevice(), depthImage, nullptr);
	vkFreeMemory(VulkanCore::getDevice(), depthImageMemory, nullptr);
}

void MousePick::recreateMousePick() {
	if (mousePickExtent.width == 0 || mousePickExtent.height == 0)
		return;

	cleanupFramebuffers();
	createMousePickImage();
	createMousePickImageViews();
	createDepthResources();
	createMousePickFramebuffers();
}

void MousePick::cleanup() {
	// MeshManager cleanup is handled by ResourceContext

	for (auto framebuffer : mousePickFramebuffers) {
		vkDestroyFramebuffer(VulkanCore::getDevice(), framebuffer, nullptr);
	}

	for (auto imageView : mousePickImageViews) {
		vkDestroyImageView(VulkanCore::getDevice(), imageView, nullptr);
	}

	for (auto image : mousePickImages) {
		vkDestroyImage(VulkanCore::getDevice(), image, nullptr);
	}

	for (auto memory : mousePickImageMemory) {
		vkFreeMemory(VulkanCore::getDevice(), memory, nullptr);
	}

	vkDestroyImageView(VulkanCore::getDevice(), depthImageView, nullptr);
	vkDestroyImage(VulkanCore::getDevice(), depthImage, nullptr);
	vkFreeMemory(VulkanCore::getDevice(), depthImageMemory, nullptr);

	vkDestroyRenderPass(VulkanCore::getDevice(), mousePickRenderPass, nullptr);

	vkDestroyPipeline(VulkanCore::getDevice(), mousePickPipeline, nullptr);
}

void MousePick::createMousePickImage() {
	mousePickImages.resize(engineCore->getSwapChainImageViews().size());
	mousePickImageMemory.resize(engineCore->getSwapChainImageViews().size());

	for (uint32_t i = 0; i < engineCore->getSwapChainImageViews().size(); i++) {
		// Create the linear tiled destination image to copy to and to read the
		// memory from
		VkImageCreateInfo imageCreateCI{};
		imageCreateCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateCI.imageType = VK_IMAGE_TYPE_2D;
		// Note that vkCmdBlitImage (if supported) will also do format conversions
		// if the swapchain color format would differ
		imageCreateCI.format = VK_FORMAT_B8G8R8A8_UNORM;
		imageCreateCI.extent.width = mousePickExtent.width;
		imageCreateCI.extent.height = mousePickExtent.height;
		imageCreateCI.extent.depth = 1;
		imageCreateCI.arrayLayers = 1;
		imageCreateCI.mipLevels = 1;
		imageCreateCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateCI.usage =
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

		// Create the image
		// VkImage dstImage;
		vkCreateImage(VulkanCore::getDevice(), &imageCreateCI, nullptr, &mousePickImages[i]);
		// Create memory to back up the image
		VkMemoryRequirements memRequirements;
		VkMemoryAllocateInfo memAllocInfo{};
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		// VkDeviceMemory dstImageMemory;
		vkGetImageMemoryRequirements(VulkanCore::getDevice(), mousePickImages[i], &memRequirements);
		memAllocInfo.allocationSize = memRequirements.size;
		// Memory must be host visible to copy from
		memAllocInfo.memoryTypeIndex =
			Utils::findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		if (vkAllocateMemory(VulkanCore::getDevice(), &memAllocInfo, nullptr, &mousePickImageMemory[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to allocate image memory!");
		}
		vkBindImageMemory(VulkanCore::getDevice(), mousePickImages[i], mousePickImageMemory[i], 0);

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandPool = VulkanCore::getCommandPool();
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer copyCmd;
		vkAllocateCommandBuffers(VulkanCore::getDevice(), &allocInfo, &copyCmd);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(copyCmd, &beginInfo);

		VkImageMemoryBarrier imageMemoryBarrier{};
		imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageMemoryBarrier.image = mousePickImages[i];
		imageMemoryBarrier.subresourceRange = VkImageSubresourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

		vkCmdPipelineBarrier(copyCmd,
							 VK_PIPELINE_STAGE_TRANSFER_BIT,
							 VK_PIPELINE_STAGE_TRANSFER_BIT,
							 0,
							 0,
							 nullptr,
							 0,
							 nullptr,
							 1,
							 &imageMemoryBarrier);

		vkEndCommandBuffer(copyCmd);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &copyCmd;

		vkQueueSubmit(VulkanCore::getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(VulkanCore::getGraphicsQueue());

		vkFreeCommandBuffers(VulkanCore::getDevice(), VulkanCore::getCommandPool(), 1, &copyCmd);
	}
}

void MousePick::createMousePickImageViews() {
	mousePickImageViews.resize(mousePickImages.size());
	for (uint32_t i = 0; i < mousePickImages.size(); i++) {
		mousePickImageViews[i] =
			Utils::createImageView(mousePickImages[i], VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT, 1);
	}
}

void MousePick::createDepthResources() {
	VkFormat depthFormat = engineCore->findDepthFormat();

	Utils::createImage(mousePickExtent.width,
					   mousePickExtent.height,
					   1,
					   VK_SAMPLE_COUNT_1_BIT,
					   depthFormat,
					   VK_IMAGE_TILING_OPTIMAL,
					   VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
					   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					   depthImage,
					   depthImageMemory);
	depthImageView = Utils::createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);

	Utils::transitionImageLayout(depthImage,
								 depthFormat,
								 VK_IMAGE_LAYOUT_UNDEFINED,
								 VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
								 1,
								 VulkanCore::getCommandPool());
}

void MousePick::createMousePickRenderPass() {
	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = VK_FORMAT_B8G8R8A8_UNORM;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = engineCore->findDepthFormat();
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependency.dstStageMask =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(VulkanCore::getDevice(), &renderPassInfo, nullptr, &mousePickRenderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}
}

void MousePick::createMousePickFramebuffers() {
	mousePickFramebuffers.resize(mousePickImageViews.size());

	for (size_t i = 0; i < mousePickImageViews.size(); i++) {
		std::array<VkImageView, 2> attachments = {mousePickImageViews[i], depthImageView};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = mousePickRenderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = mousePickExtent.width;
		framebufferInfo.height = mousePickExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(VulkanCore::getDevice(), &framebufferInfo, nullptr, &mousePickFramebuffers[i]) !=
			VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
}

void MousePick::recordMousePickCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
	this->imageIndex = imageIndex;

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	// beginInfo.flags = 0;
	// // Optional beginInfo.pInheritanceInfo = nullptr; // Optional

	if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = mousePickRenderPass;
	renderPassInfo.framebuffer = mousePickFramebuffers[imageIndex];
	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = mousePickExtent;

	std::array<VkClearValue, 2> clearValues{};
	// Clear color attachment to transparent black (no entity)
	clearValues[0].color = {{0.0f, 0.0f, 0.0f, 0.0f}};
	// Clear depth to far plane so closest fragments pass depth test
	clearValues[1].depthStencil.depth = 1.0f;
	clearValues[1].depthStencil.stencil = 0;

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)mousePickExtent.width;
	viewport.height = (float)mousePickExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.offset = {0, 0};
	scissor.extent = mousePickExtent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	SceneRenderer::renderMousePick(commandBuffer,
								   mousePickPipeline,
								   engineCore->getPipelineLayout(),
								   VulkanCore::getCurrentFrame(),
								   VulkanCore::getDynamicAlignment());

	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}
}

void MousePick::createMousePickCommandBuffers() {
	mousePickCommandBuffers.resize(mousePickImageViews.size());

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = VulkanCore::getCommandPool();
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(mousePickCommandBuffers.size());

	if (vkAllocateCommandBuffers(VulkanCore::getDevice(), &allocInfo, mousePickCommandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate viewport command buffers!");
	}
}

uint32_t MousePick::getEntityIDAt(int x, int y) {
	VkImage srcImage = mousePickImages[imageIndex];
	VkDevice device = VulkanCore::getDevice();

	// Create a buffer to copy the pixel to
	VkDeviceSize pixelSize = 4; // BGRA8
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = pixelSize;
	bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vkCreateBuffer(device, &bufferInfo, nullptr, &stagingBuffer);

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = Utils::findMemoryType(
		memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory);
	vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);

	// Copy the pixel from the image to the buffer
	VkCommandBufferAllocateInfo cmdBufAllocInfo{};
	cmdBufAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocInfo.commandPool = VulkanCore::getCommandPool();
	cmdBufAllocInfo.commandBufferCount = 1;

	VkCommandBuffer cmdBuffer;
	vkAllocateCommandBuffers(device, &cmdBufAllocInfo, &cmdBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmdBuffer, &beginInfo);

	// Transition image layout to VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = srcImage;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	vkCmdPipelineBarrier(cmdBuffer,
						 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // or appropriate stage
						 VK_PIPELINE_STAGE_TRANSFER_BIT,
						 0,
						 0,
						 nullptr,
						 0,
						 nullptr,
						 1,
						 &barrier);

	VkBufferImageCopy region{};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = {x, y, 0};
	region.imageExtent = {1, 1, 1};

	vkCmdCopyImageToBuffer(cmdBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);

	// Transition image layout back to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	VkImageMemoryBarrier barrierBack{};
	barrierBack.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrierBack.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	barrierBack.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrierBack.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrierBack.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrierBack.image = srcImage;
	barrierBack.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrierBack.subresourceRange.baseMipLevel = 0;
	barrierBack.subresourceRange.levelCount = 1;
	barrierBack.subresourceRange.baseArrayLayer = 0;
	barrierBack.subresourceRange.layerCount = 1;
	barrierBack.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	barrierBack.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(cmdBuffer,
						 VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						 0,
						 0,
						 nullptr,
						 0,
						 nullptr,
						 1,
						 &barrierBack);

	vkEndCommandBuffer(cmdBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;

	vkQueueSubmit(VulkanCore::getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(VulkanCore::getGraphicsQueue());

	// Map and read the pixel
	uint8_t* data;
	vkMapMemory(device, stagingBufferMemory, 0, pixelSize, 0, (void**)&data);

	// BGRA8 to entity ID (assuming ID is encoded as R,G,B)
	uint32_t b = data[0];
	uint32_t g = data[1];
	uint32_t r = data[2];
	uint32_t a = data[3];

	vkUnmapMemory(device, stagingBufferMemory);

	// Cleanup
	vkFreeCommandBuffers(device, VulkanCore::getCommandPool(), 1, &cmdBuffer);
	vkDestroyBuffer(device, stagingBuffer, nullptr);
	vkFreeMemory(device, stagingBufferMemory, nullptr);

	if (a == 0x00)
		return static_cast<uint32_t>(-1); // No entity

	uint32_t entityID = r | (g << 8) | (b << 16);

	return entityID;
}

std::vector<VkImageView> MousePick::getMousePickImageViews() const {
	return mousePickImageViews;
}

VkExtent2D MousePick::getMousePickExtent() const {
	return mousePickExtent;
}

void MousePick::createGraphicsPipeline() {
	auto vertShaderCode = Utils::readFile("shaders/vert.spv");
	auto fragShaderCode = Utils::readFile("shaders/frag.spv");

	VkShaderModule vertShaderModule;
	VkShaderModule fragShaderModule;

	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = vertShaderCode.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());

	if (vkCreateShaderModule(VulkanCore::getDevice(), &createInfo, nullptr, &vertShaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module!");
	}

	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = fragShaderCode.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());

	if (vkCreateShaderModule(VulkanCore::getDevice(), &createInfo, nullptr, &fragShaderModule) != VK_SUCCESS) {
		throw std::runtime_error("failed to create shader module!");
	}

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
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	vertexInputInfo.vertexBindingDescriptionCount = 1;
	vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

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

	std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
	pipelineInfo.basePipelineHandle = engineCore->getPipeline();
	pipelineInfo.basePipelineIndex = -1;
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
	pipelineInfo.layout = engineCore->getPipelineLayout();
	pipelineInfo.renderPass = mousePickRenderPass;
	pipelineInfo.subpass = 0;

	if (vkCreateGraphicsPipelines(
			VulkanCore::getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &mousePickPipeline) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics pipeline!");
	}

	vkDestroyShaderModule(VulkanCore::getDevice(), fragShaderModule, nullptr);
	vkDestroyShaderModule(VulkanCore::getDevice(), vertShaderModule, nullptr);
}
