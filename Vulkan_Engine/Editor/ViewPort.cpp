#include "ViewPort.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <stdexcept>

#include "core/utils/Utils.h"
#include "core/vulkancore.h"
#include "gridPlane/GridPlane.h"
#include "renderer/vulkan/VulkanCommandList.h"
#include "renderer/vulkan/VulkanDevice.h"
#include "SceneRenderer.h"
#include "skybox/Skybox.h"
#include "vulkan/vulkan_core.h"

#include "glm/ext/matrix_float4x4.hpp"

void ViewPort::init(Renderer::VulkanDevice* device,
					VkExtent2D viewportExtent,
					VkImageView skyboxView,
					VkSampler skyboxSampler,
					bool gameViewport) {
	vulkanDevice = device;
	this->viewportExtent = viewportExtent;
	this->isGameViewport = gameViewport;

	VkCommandPoolCreateInfo commandPoolCreateInfo{};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.queueFamilyIndex = vulkanDevice->getGraphicsQueueFamily();
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(vulkanDevice->getDevice(), &commandPoolCreateInfo, nullptr, &m_ViewportCommandPool) !=
		VK_SUCCESS) {
		throw std::runtime_error("Could not create graphics command pool");
	}

	createColorResources();
	createDepthResources();

	m_ViewportCommandBuffers.resize(vulkanDevice->getSwapChainImageCount());

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool = m_ViewportCommandPool;
	allocInfo.commandBufferCount = (uint32_t)m_ViewportCommandBuffers.size();

	if (vkAllocateCommandBuffers(vulkanDevice->getDevice(), &allocInfo, m_ViewportCommandBuffers.data()) !=
		VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	}

	createViewportRenderPass();
	createViewportFramebuffers();

	// Only init singletons for the Editor viewport; Game viewport reuses them
	if (!isGameViewport) {
		GridPlane::init(vulkanDevice, m_ViewportCommandPool, m_ViewportRenderPass);
		Skybox::init(vulkanDevice, m_ViewportCommandPool, m_ViewportRenderPass, skyboxView, skyboxSampler);
	}
}

void ViewPort::cleanupFramebuffers() {
	for (auto framebuffer : m_ViewportFramebuffers) {
		vkDestroyFramebuffer(vulkanDevice->getDevice(), framebuffer, nullptr);
	}

	vkDestroyImageView(vulkanDevice->getDevice(), depthImageView, nullptr);
	vkDestroyImage(vulkanDevice->getDevice(), depthImage, nullptr);
	vkFreeMemory(vulkanDevice->getDevice(), depthImageMemory, nullptr);

	vkDestroyImageView(vulkanDevice->getDevice(), colorImageView, nullptr);
	vkDestroyImage(vulkanDevice->getDevice(), colorImage, nullptr);
	vkFreeMemory(vulkanDevice->getDevice(), colorImageMemory, nullptr);

	vkDestroyImageView(vulkanDevice->getDevice(), hdrResolveImageView, nullptr);
	vkDestroyImage(vulkanDevice->getDevice(), hdrResolveImage, nullptr);
	vkFreeMemory(vulkanDevice->getDevice(), hdrResolveImageMemory, nullptr);
}

void ViewPort::recreateViewport(VkExtent2D viewportExtent) {
	if (viewportExtent.width == 0 || viewportExtent.height == 0)
		return;

	cleanupFramebuffers();
	this->viewportExtent = viewportExtent;
	createColorResources();
	createDepthResources();
	createViewportFramebuffers();
}

void ViewPort::cleanup() {
	// Only the editor viewport owns the GridPlane and Skybox singletons
	if (!isGameViewport) {
		GridPlane::cleanup();
		Skybox::cleanup();
	}

	for (auto framebuffer : m_ViewportFramebuffers) {
		vkDestroyFramebuffer(vulkanDevice->getDevice(), framebuffer, nullptr);
	}

	vkDestroyImageView(vulkanDevice->getDevice(), depthImageView, nullptr);
	vkDestroyImage(vulkanDevice->getDevice(), depthImage, nullptr);
	vkFreeMemory(vulkanDevice->getDevice(), depthImageMemory, nullptr);

	vkDestroyImageView(vulkanDevice->getDevice(), colorImageView, nullptr);
	vkDestroyImage(vulkanDevice->getDevice(), colorImage, nullptr);
	vkFreeMemory(vulkanDevice->getDevice(), colorImageMemory, nullptr);

	vkDestroyImageView(vulkanDevice->getDevice(), hdrResolveImageView, nullptr);
	vkDestroyImage(vulkanDevice->getDevice(), hdrResolveImage, nullptr);
	vkFreeMemory(vulkanDevice->getDevice(), hdrResolveImageMemory, nullptr);

	vkDestroyCommandPool(vulkanDevice->getDevice(), m_ViewportCommandPool, nullptr);

	vkDestroyRenderPass(vulkanDevice->getDevice(), m_ViewportRenderPass, nullptr);
}

void ViewPort::createColorResources() {
	VkFormat hdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

	Utils::createImage(viewportExtent.width,
					   viewportExtent.height,
					   1,
					   static_cast<VkSampleCountFlagBits>(vulkanDevice->getMsaaSamples()),
					   hdrFormat,
					   VK_IMAGE_TILING_OPTIMAL,
					   VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
					   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					   colorImage,
					   colorImageMemory);
	colorImageView = Utils::createImageView(colorImage, hdrFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	Utils::createImage(viewportExtent.width,
					   viewportExtent.height,
					   1,
					   VK_SAMPLE_COUNT_1_BIT,
					   hdrFormat,
					   VK_IMAGE_TILING_OPTIMAL,
					   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					   hdrResolveImage,
					   hdrResolveImageMemory);
	hdrResolveImageView = Utils::createImageView(hdrResolveImage, hdrFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
}

void ViewPort::createDepthResources() {
	VkFormat depthFormat = vulkanDevice->findDepthFormat();

	Utils::createImage(viewportExtent.width,
					   viewportExtent.height,
					   1,
					   static_cast<VkSampleCountFlagBits>(vulkanDevice->getMsaaSamples()),
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
								 m_ViewportCommandPool);
}

void ViewPort::createViewportRenderPass() {
	VkFormat hdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

	VkAttachmentDescription colorAttachment{};
	colorAttachment.format = hdrFormat;
	colorAttachment.samples = static_cast<VkSampleCountFlagBits>(vulkanDevice->getMsaaSamples());
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachment{};
	depthAttachment.format = vulkanDevice->findDepthFormat();
	depthAttachment.samples = static_cast<VkSampleCountFlagBits>(vulkanDevice->getMsaaSamples());
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription colorAttachmentResolve{};
	colorAttachmentResolve.format = hdrFormat;
	colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Optimized for reading in HDR pass

	VkAttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentResolveRef{};
	colorAttachmentResolveRef.attachment = 2;
	colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;
	subpass.pResolveAttachments = &colorAttachmentResolveRef;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependency.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependency.dstStageMask =
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 3> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve};
	VkRenderPassCreateInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(vulkanDevice->getDevice(), &renderPassInfo, nullptr, &m_ViewportRenderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}
}

void ViewPort::createViewportFramebuffers() {
	m_ViewportFramebuffers.resize(vulkanDevice->getSwapChainImageCount());

	for (size_t i = 0; i < vulkanDevice->getSwapChainImageCount(); i++) {
		std::array<VkImageView, 3> attachments = {
			colorImageView,
			depthImageView,
			hdrResolveImageView,
		};

		VkFramebufferCreateInfo framebufferInfo{};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_ViewportRenderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = viewportExtent.width;
		framebufferInfo.height = viewportExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(vulkanDevice->getDevice(), &framebufferInfo, nullptr, &m_ViewportFramebuffers[i]) !=
			VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
}

void ViewPort::recordViewportCommandBuffer(VkCommandBuffer commandBuffer,
										   uint32_t imageIndex,
										   const glm::mat4& lightSpaceMatrix,
										   VkDescriptorSet globalDescriptorSet) {
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	if (vkBeginCommandBuffer(m_ViewportCommandBuffers[VulkanCore::getCurrentFrame()], &beginInfo) != VK_SUCCESS) {
		throw std::runtime_error("failed to begin recording command buffer!");
	}

	VkRenderPassBeginInfo renderPassInfo{};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = m_ViewportRenderPass;
	renderPassInfo.framebuffer = m_ViewportFramebuffers[imageIndex];
	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent = viewportExtent;

	std::array<VkClearValue, 2> clearValues{};
	clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
	clearValues[1].depthStencil = {1.0f, 0};

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(
		m_ViewportCommandBuffers[VulkanCore::getCurrentFrame()], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

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

	// --- Bind GlobalUBO descriptor set at set=0, shared across all draws in this pass ---
	vkCmdBindDescriptorSets(commandBuffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							vulkanDevice->getPipelineLayout(),
							0 /*firstSet*/,
							1,
							&globalDescriptorSet,
							0,
							nullptr);

	VkDeviceSize offsets[] = {0};
	uint32_t skyboxDynamicOffset[] = {0};
	uint32_t gridPlaneDynamicOffset[] = {0, 0};

	// Skybox draws in both editor and game viewports
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &Skybox::getSkyboxVertexBuffer(), offsets);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Skybox::getSkyboxPipeline());
	vkCmdBindDescriptorSets(commandBuffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							Skybox::getSkyboxPipelineLayout(),
							0,
							1,
							Skybox::getSkyboxDescriptorSet().data(),
							1,
							skyboxDynamicOffset);
	vkCmdDraw(commandBuffer, 36, 1, 0, 0);

	// Push the directional light view-projection matrix at offset 16 (after pickColor+usePickColor)
	vkCmdPushConstants(m_ViewportCommandBuffers[VulkanCore::getCurrentFrame()],
					   vulkanDevice->getPipelineLayout(),
					   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					   16, // offset: skip the 16-byte pick color block
					   sizeof(glm::mat4), // 64 bytes
					   &lightSpaceMatrix);

	Renderer::VulkanCommandList commandList(commandBuffer);
	SceneRenderer::renderScene(commandList,
							   vulkanDevice->getPipeline(),
							   vulkanDevice->getPipelineLayout(),
							   VulkanCore::getCurrentFrame(),
							   globalDescriptorSet);

	// Editor viewport draws GridPlane (no grid in game view)
	if (!isGameViewport) {
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GridPlane::getGridPlanePipeline());
		vkCmdBindDescriptorSets(commandBuffer,
								VK_PIPELINE_BIND_POINT_GRAPHICS,
								GridPlane::getGridPlanePipelineLayout(),
								0,
								1,
								GridPlane::getGridPlaneDescriptorSets().data(),
								2,
								gridPlaneDynamicOffset);
		vkCmdDraw(commandBuffer, 4, 1, 0, 0);
	}

	vkCmdEndRenderPass(commandBuffer);

	if (vkEndCommandBuffer(m_ViewportCommandBuffers[VulkanCore::getCurrentFrame()]) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}
}

void ViewPort::createViewportCommandBuffers() {
	m_ViewportCommandBuffers.resize(vulkanDevice->getSwapChainImageCount());

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_ViewportCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = static_cast<uint32_t>(m_ViewportCommandBuffers.size());

	if (vkAllocateCommandBuffers(vulkanDevice->getDevice(), &allocInfo, m_ViewportCommandBuffers.data()) !=
		VK_SUCCESS) {
		throw std::runtime_error("failed to allocate viewport command buffers!");
	}
}
