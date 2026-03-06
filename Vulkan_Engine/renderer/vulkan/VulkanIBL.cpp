#include "VulkanIBL.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>
#include <stdexcept>
#include <string>

#include "core/utils/Utils.h"
#include "renderer/vulkan/VulkanDevice.h"
#include "vulkan/vulkan_core.h"

#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/trigonometric.hpp"

// ---------------------------------------------------------------------------
// Cube-face capture: view matrices identical to LearnOpenGL captureViews[6]
// ---------------------------------------------------------------------------
static std::array<glm::mat4, 6> makeCaptureViews() {
	return {
		glm::lookAt(glm::vec3(0), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)), // +X
		glm::lookAt(glm::vec3(0), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)), // -X
		glm::lookAt(glm::vec3(0), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)), // +Y
		glm::lookAt(glm::vec3(0), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)), // -Y
		glm::lookAt(glm::vec3(0), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)), // +Z
		glm::lookAt(glm::vec3(0), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f)) // -Z
	};
}

// Push constant layout must exactly match cubemap.vert
struct CapturePushConstants {
	glm::mat4 view;
	glm::mat4 proj;
};

// ============================================================================
void VulkanIBL::init(Renderer::VulkanDevice* device, VkCommandPool commandPool, const char* hdrPath) {
	vulkanDevice = device;

	createCubemapSampler();
	loadEquirect(commandPool, hdrPath);
	buildEnvCubemap(commandPool);
	buildIrradianceMap(commandPool);

	// Temp HDR 2-D image is no longer needed after buildEnvCubemap
	vkDestroySampler(vulkanDevice->getDevice(), hdrSampler, nullptr);
	hdrSampler = VK_NULL_HANDLE;
	vkDestroyImageView(vulkanDevice->getDevice(), hdrImageView, nullptr);
	hdrImageView = VK_NULL_HANDLE;
	vkDestroyImage(vulkanDevice->getDevice(), hdrImage, nullptr);
	hdrImage = VK_NULL_HANDLE;
	vkFreeMemory(vulkanDevice->getDevice(), hdrMemory, nullptr);
	hdrMemory = VK_NULL_HANDLE;

	// Env cubemap view is still kept for possible future specular IBL reuse
	// but we no longer need to render it each frame.
}

void VulkanIBL::cleanup() {
	VkDevice dev = vulkanDevice->getDevice();
	vkDestroySampler(dev, cubemapSampler, nullptr);

	vkDestroyImageView(dev, irradianceImageView, nullptr);
	vkDestroyImage(dev, irradianceImage, nullptr);
	vkFreeMemory(dev, irradianceMemory, nullptr);

	vkDestroyImageView(dev, envCubemapImageView, nullptr);
	vkDestroyImage(dev, envCubemapImage, nullptr);
	vkFreeMemory(dev, envCubemapMemory, nullptr);
}

// ============================================================================
// Private helpers
// ============================================================================

void VulkanIBL::createCubemapSampler() {
	VkSamplerCreateInfo ci{};
	ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	ci.magFilter = VK_FILTER_LINEAR;
	ci.minFilter = VK_FILTER_LINEAR;
	ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	ci.minLod = 0.0f;
	ci.maxLod = VK_LOD_CLAMP_NONE;
	ci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	if (vkCreateSampler(vulkanDevice->getDevice(), &ci, nullptr, &cubemapSampler) != VK_SUCCESS)
		throw std::runtime_error("VulkanIBL: failed to create cubemap sampler");
}

// ----------------------------------------------------------------------------
// Load the equirectangular HDR into a 2-D VkImage (RGBA32F)
// ----------------------------------------------------------------------------
void VulkanIBL::loadEquirect(VkCommandPool commandPool, const char* hdrPath) {
	int w, h, channels;
	stbi_set_flip_vertically_on_load(true);
	float* pixels = stbi_loadf(hdrPath, &w, &h, &channels, STBI_rgb_alpha);
	if (!pixels)
		throw std::runtime_error(std::string("VulkanIBL: failed to load HDR map: ") + hdrPath);

	VkDeviceSize imageSize = static_cast<VkDeviceSize>(w) * h * 4 * sizeof(float);

	// Staging buffer
	VkBuffer stagingBuf;
	VkDeviceMemory stagingMem;
	Utils::createBuffer(imageSize,
						VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
						stagingBuf,
						stagingMem);
	void* data;
	vkMapMemory(vulkanDevice->getDevice(), stagingMem, 0, imageSize, 0, &data);
	memcpy(data, pixels, imageSize);
	vkUnmapMemory(vulkanDevice->getDevice(), stagingMem);
	stbi_image_free(pixels);

	// Create a 2D VkImage (RGBA32F)
	Utils::createImage(static_cast<uint32_t>(w),
					   static_cast<uint32_t>(h),
					   1,
					   VK_SAMPLE_COUNT_1_BIT,
					   VK_FORMAT_R32G32B32A32_SFLOAT,
					   VK_IMAGE_TILING_OPTIMAL,
					   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					   hdrImage,
					   hdrMemory);

	Utils::transitionImageLayout(hdrImage,
								 VK_FORMAT_R32G32B32A32_SFLOAT,
								 VK_IMAGE_LAYOUT_UNDEFINED,
								 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								 1,
								 commandPool);

	// Copy staging → image
	VkCommandBuffer cmd = Utils::beginSingleTimeCommands(commandPool);
	VkBufferImageCopy region{};
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
	vkCmdCopyBufferToImage(cmd, stagingBuf, hdrImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
	Utils::endSingleTimeCommands(commandPool, cmd);

	Utils::transitionImageLayout(hdrImage,
								 VK_FORMAT_R32G32B32A32_SFLOAT,
								 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
								 1,
								 commandPool);

	hdrImageView = Utils::createImageView(hdrImage, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	// HDR sampler (linear, clamp)
	VkSamplerCreateInfo sci{};
	sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sci.magFilter = VK_FILTER_LINEAR;
	sci.minFilter = VK_FILTER_LINEAR;
	sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sci.maxLod = VK_LOD_CLAMP_NONE;
	if (vkCreateSampler(vulkanDevice->getDevice(), &sci, nullptr, &hdrSampler) != VK_SUCCESS)
		throw std::runtime_error("VulkanIBL: failed to create HDR sampler");

	vkDestroyBuffer(vulkanDevice->getDevice(), stagingBuf, nullptr);
	vkFreeMemory(vulkanDevice->getDevice(), stagingMem, nullptr);
}

// ----------------------------------------------------------------------------
// Allocate + create a cube VkImage with 6 array layers (RGBA16F) and mipmaps
// ----------------------------------------------------------------------------
void VulkanIBL::createCubemapImage(
	uint32_t size, VkImage& img, VkDeviceMemory& mem, VkImageView& view, uint32_t mipLevels) {
	Utils::createImage(size,
					   size,
					   mipLevels,
					   VK_SAMPLE_COUNT_1_BIT,
					   VK_FORMAT_R16G16B16A16_SFLOAT,
					   VK_IMAGE_TILING_OPTIMAL,
					   VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
						   VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
					   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					   img,
					   mem,
					   6, // 6 array layers
					   VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

	view = Utils::createImageView(
		img, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, VK_IMAGE_VIEW_TYPE_CUBE, 6);
}

// ----------------------------------------------------------------------------
// Transition all 6 faces of a cubemap image between layouts
// ----------------------------------------------------------------------------
void VulkanIBL::transitionCubemap(
	VkCommandPool commandPool, VkImage img, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels) {
	Utils::transitionImageLayout(img,
								 VK_FORMAT_R16G16B16A16_SFLOAT,
								 oldLayout,
								 newLayout,
								 mipLevels, // mip levels
								 commandPool,
								 6); // 6 array layers
}

// ----------------------------------------------------------------------------
// Generate mipmaps for a cubemap image
// ----------------------------------------------------------------------------
void VulkanIBL::generateMipmaps(VkCommandPool commandPool,
								VkImage image,
								VkFormat imageFormat,
								int32_t texWidth,
								int32_t texHeight,
								uint32_t mipLevels,
								uint32_t layerCount) {
	// Check if image format supports linear blitting
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(vulkanDevice->getPhysicalDevice(), imageFormat, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("VulkanIBL: texture image format does not support linear blitting!");
	}

	VkCommandBuffer commandBuffer = Utils::beginSingleTimeCommands(commandPool);

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = layerCount;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
							 VK_PIPELINE_STAGE_TRANSFER_BIT,
							 VK_PIPELINE_STAGE_TRANSFER_BIT,
							 0,
							 0,
							 nullptr,
							 0,
							 nullptr,
							 1,
							 &barrier);

		VkImageBlit imageBlit{};
		imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.srcSubresource.layerCount = layerCount;
		imageBlit.srcSubresource.baseArrayLayer = 0;
		imageBlit.srcSubresource.mipLevel = i - 1;
		imageBlit.srcOffsets[0] = {0, 0, 0};
		imageBlit.srcOffsets[1] = {mipWidth, mipHeight, 1};

		imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.dstSubresource.layerCount = layerCount;
		imageBlit.dstSubresource.baseArrayLayer = 0;
		imageBlit.dstSubresource.mipLevel = i;
		imageBlit.dstOffsets[0] = {0, 0, 0};
		imageBlit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};

		vkCmdBlitImage(commandBuffer,
					   image,
					   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					   image,
					   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					   1,
					   &imageBlit,
					   VK_FILTER_LINEAR);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
							 VK_PIPELINE_STAGE_TRANSFER_BIT,
							 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
							 0,
							 0,
							 nullptr,
							 0,
							 nullptr,
							 1,
							 &barrier);

		if (mipWidth > 1)
			mipWidth /= 2;
		if (mipHeight > 1)
			mipHeight /= 2;
	}

	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer,
						 VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						 0,
						 0,
						 nullptr,
						 0,
						 nullptr,
						 1,
						 &barrier);

	Utils::endSingleTimeCommands(commandPool, commandBuffer);
}

// ----------------------------------------------------------------------------
// Core offline render: renders each of the 6 faces using a push-constant
// that selects one of the 6 captureViews.
// ----------------------------------------------------------------------------
void VulkanIBL::renderCubemapFaces(VkCommandPool commandPool,
								   uint32_t size,
								   const char* vertSpv,
								   const char* fragSpv,
								   VkDescriptorSetLayout descLayout,
								   VkDescriptorSet descSet,
								   VkImage dstImage,
								   uint32_t mipLevel) {
	VkDevice dev = vulkanDevice->getDevice();

	// ---------- Render pass ----------
	VkAttachmentDescription colorAtt{};
	colorAtt.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	colorAtt.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAtt.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAtt.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAtt.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAtt.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorRef{};
	colorRef.attachment = 0;
	colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass{};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorRef;

	VkSubpassDependency dep{};
	dep.srcSubpass = VK_SUBPASS_EXTERNAL;
	dep.dstSubpass = 0;
	dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.srcAccessMask = 0;
	dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo rpCI{};
	rpCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpCI.attachmentCount = 1;
	rpCI.pAttachments = &colorAtt;
	rpCI.subpassCount = 1;
	rpCI.pSubpasses = &subpass;
	rpCI.dependencyCount = 1;
	rpCI.pDependencies = &dep;

	VkRenderPass renderPass;
	if (vkCreateRenderPass(dev, &rpCI, nullptr, &renderPass) != VK_SUCCESS)
		throw std::runtime_error("VulkanIBL: failed to create capture render pass");

	// ---------- Pipeline layout ----------
	VkPushConstantRange pcRange{};
	pcRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pcRange.offset = 0;
	pcRange.size = sizeof(CapturePushConstants);

	VkPipelineLayoutCreateInfo plCI{};
	plCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plCI.setLayoutCount = 1;
	plCI.pSetLayouts = &descLayout;
	plCI.pushConstantRangeCount = 1;
	plCI.pPushConstantRanges = &pcRange;

	VkPipelineLayout pipelineLayout;
	if (vkCreatePipelineLayout(dev, &plCI, nullptr, &pipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("VulkanIBL: failed to create capture pipeline layout");

	// ---------- Shaders ----------
	auto vertCode = Utils::readFile(vertSpv);
	auto fragCode = Utils::readFile(fragSpv);
	VkShaderModule vertModule, fragModule;
	Utils::createShaderModule(vertCode, vertModule);
	Utils::createShaderModule(fragCode, fragModule);

	VkPipelineShaderStageCreateInfo stages[2]{};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vertModule;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fragModule;
	stages[1].pName = "main";

	// ---------- Pipeline ----------
	VkPipelineVertexInputStateCreateInfo viCI{};
	viCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkPipelineInputAssemblyStateCreateInfo iaCI{};
	iaCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	iaCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkViewport viewport{0, 0, (float)size, (float)size, 0, 1};
	VkRect2D scissor{{0, 0}, {size, size}};

	VkPipelineViewportStateCreateInfo vpsCI{};
	vpsCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vpsCI.viewportCount = 1;
	vpsCI.pViewports = &viewport;
	vpsCI.scissorCount = 1;
	vpsCI.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rastCI{};
	rastCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rastCI.polygonMode = VK_POLYGON_MODE_FILL;
	rastCI.cullMode = VK_CULL_MODE_NONE;
	rastCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rastCI.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo msCI{};
	msCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	msCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState cba{};
	cba.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo cbCI{};
	cbCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cbCI.attachmentCount = 1;
	cbCI.pAttachments = &cba;

	VkGraphicsPipelineCreateInfo gpCI{};
	gpCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gpCI.stageCount = 2;
	gpCI.pStages = stages;
	gpCI.pVertexInputState = &viCI;
	gpCI.pInputAssemblyState = &iaCI;
	gpCI.pViewportState = &vpsCI;
	gpCI.pRasterizationState = &rastCI;
	gpCI.pMultisampleState = &msCI;
	gpCI.pColorBlendState = &cbCI;
	gpCI.layout = pipelineLayout;
	gpCI.renderPass = renderPass;
	gpCI.subpass = 0;

	VkPipeline pipeline;
	if (vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpCI, nullptr, &pipeline) != VK_SUCCESS)
		throw std::runtime_error("VulkanIBL: failed to create capture pipeline");

	vkDestroyShaderModule(dev, fragModule, nullptr);
	vkDestroyShaderModule(dev, vertModule, nullptr);

	// ---------- Capture: render each of the 6 faces ----------
	auto captureViews = makeCaptureViews();
	glm::mat4 captureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

	// We need per-face image views to use as framebuffer attachments
	for (uint32_t face = 0; face < 6; ++face) {
		// --- Per-face 2-D image view ---
		VkImageViewCreateInfo faceViewCI{};
		faceViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		faceViewCI.image = dstImage;
		faceViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		faceViewCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		faceViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		faceViewCI.subresourceRange.baseMipLevel = mipLevel;
		faceViewCI.subresourceRange.levelCount = 1;
		faceViewCI.subresourceRange.baseArrayLayer = face;
		faceViewCI.subresourceRange.layerCount = 1;

		VkImageView faceView;
		vkCreateImageView(dev, &faceViewCI, nullptr, &faceView);

		// --- Framebuffer ---
		VkFramebufferCreateInfo fbCI{};
		fbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbCI.renderPass = renderPass;
		fbCI.attachmentCount = 1;
		fbCI.pAttachments = &faceView;
		fbCI.width = size;
		fbCI.height = size;
		fbCI.layers = 1;

		VkFramebuffer fb;
		vkCreateFramebuffer(dev, &fbCI, nullptr, &fb);

		// --- Record single-shot command buffer ---
		VkCommandBuffer cmd = Utils::beginSingleTimeCommands(commandPool);

		VkRenderPassBeginInfo rpBegin{};
		rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		rpBegin.renderPass = renderPass;
		rpBegin.framebuffer = fb;
		rpBegin.renderArea.extent = {size, size};
		VkClearValue clear{};
		clear.color = {{0, 0, 0, 1}};
		rpBegin.clearValueCount = 1;
		rpBegin.pClearValues = &clear;

		vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descSet, 0, nullptr);

		CapturePushConstants pc{};
		pc.view = captureViews[face];
		pc.proj = captureProj;
		vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

		vkCmdDraw(cmd, 36, 1, 0, 0); // 6 faces × 6 verts, all provided in shader
		vkCmdEndRenderPass(cmd);
		Utils::endSingleTimeCommands(commandPool, cmd);

		// Clean up per-face resources
		vkDestroyFramebuffer(dev, fb, nullptr);
		vkDestroyImageView(dev, faceView, nullptr);
	}

	// Clean up pipeline resources (images/views/sampler live on)
	vkDestroyPipeline(dev, pipeline, nullptr);
	vkDestroyPipelineLayout(dev, pipelineLayout, nullptr);
	vkDestroyRenderPass(dev, renderPass, nullptr);
}

// ----------------------------------------------------------------------------
// Pass 1: equirect HDR → environment cubemap (512 × 512)
// ----------------------------------------------------------------------------
void VulkanIBL::buildEnvCubemap(VkCommandPool commandPool) {
	// 2. Create the empty 512x512 cubemap with full mip chain
	uint32_t envMipLevels = static_cast<uint32_t>(std::floor(std::log2(512))) + 1;
	createCubemapImage(512, envCubemapImage, envCubemapMemory, envCubemapImageView, envMipLevels);
	transitionCubemap(commandPool,
					  envCubemapImage,
					  VK_IMAGE_LAYOUT_UNDEFINED,
					  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					  envMipLevels);

	VkDevice dev = vulkanDevice->getDevice();

	// Descriptor pool + layout + set — 1 combined image sampler (hdrTexture)
	VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
	VkDescriptorPoolCreateInfo dpCI{};
	dpCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpCI.maxSets = 1;
	dpCI.poolSizeCount = 1;
	dpCI.pPoolSizes = &ps;
	VkDescriptorPool pool;
	vkCreateDescriptorPool(dev, &dpCI, nullptr, &pool);

	VkDescriptorSetLayoutBinding binding0{};
	binding0.binding = 0;
	binding0.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding0.descriptorCount = 1;
	binding0.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo dslCI{};
	dslCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslCI.bindingCount = 1;
	dslCI.pBindings = &binding0;
	VkDescriptorSetLayout descLayout;
	vkCreateDescriptorSetLayout(dev, &dslCI, nullptr, &descLayout);

	VkDescriptorSetAllocateInfo dsAI{};
	dsAI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dsAI.descriptorPool = pool;
	dsAI.descriptorSetCount = 1;
	dsAI.pSetLayouts = &descLayout;
	VkDescriptorSet descSet;
	vkAllocateDescriptorSets(dev, &dsAI, &descSet);

	VkDescriptorImageInfo imgInfo{};
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo.imageView = hdrImageView;
	imgInfo.sampler = hdrSampler;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = descSet;
	write.dstBinding = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.pImageInfo = &imgInfo;
	vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);

	renderCubemapFaces(commandPool,
					   512,
					   "shaders/ibl/cubemap_vert.spv",
					   "shaders/ibl/equirect_to_cubemap_frag.spv",
					   descLayout,
					   descSet,
					   envCubemapImage,
					   0); // Render to base mip level 0

	// 5. Generate mipmaps for the environment cubemap before using it for convolution/prefilter
	generateMipmaps(commandPool, envCubemapImage, VK_FORMAT_R16G16B16A16_SFLOAT, 512, 512, envMipLevels, 6);

	// The mipmap generation already transitions all levels and layers to SHADER_READ_ONLY_OPTIMAL

	vkDestroyDescriptorSetLayout(dev, descLayout, nullptr);
	vkDestroyDescriptorPool(dev, pool, nullptr);
}

// ----------------------------------------------------------------------------
// Pass 2: environment cubemap → irradiance cubemap (32 × 32)
// ----------------------------------------------------------------------------
void VulkanIBL::buildIrradianceMap(VkCommandPool commandPool) {
	// 1. Create a 32x32 irradiance cubemap (only 1 mip level needed for irradiance)
	createCubemapImage(32, irradianceImage, irradianceMemory, irradianceImageView, 1);
	transitionCubemap(
		commandPool, irradianceImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 1);

	VkDevice dev = vulkanDevice->getDevice();

	// Descriptor pool + layout + set — 1 samplerCube (environmentMap)
	VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
	VkDescriptorPoolCreateInfo dpCI{};
	dpCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpCI.maxSets = 1;
	dpCI.poolSizeCount = 1;
	dpCI.pPoolSizes = &ps;
	VkDescriptorPool pool;
	vkCreateDescriptorPool(dev, &dpCI, nullptr, &pool);

	VkDescriptorSetLayoutBinding binding0{};
	binding0.binding = 0;
	binding0.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding0.descriptorCount = 1;
	binding0.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo dslCI{};
	dslCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslCI.bindingCount = 1;
	dslCI.pBindings = &binding0;
	VkDescriptorSetLayout descLayout;
	vkCreateDescriptorSetLayout(dev, &dslCI, nullptr, &descLayout);

	VkDescriptorSetAllocateInfo dsAI{};
	dsAI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	dsAI.descriptorPool = pool;
	dsAI.descriptorSetCount = 1;
	dsAI.pSetLayouts = &descLayout;
	VkDescriptorSet descSet;
	vkAllocateDescriptorSets(dev, &dsAI, &descSet);

	VkDescriptorImageInfo imgInfo{};
	imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imgInfo.imageView = envCubemapImageView;
	imgInfo.sampler = cubemapSampler;

	VkWriteDescriptorSet write{};
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = descSet;
	write.dstBinding = 0;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.descriptorCount = 1;
	write.pImageInfo = &imgInfo;
	vkUpdateDescriptorSets(dev, 1, &write, 0, nullptr);

	renderCubemapFaces(commandPool,
					   32,
					   "shaders/ibl/cubemap_vert.spv",
					   "shaders/ibl/irradiance_conv_frag.spv",
					   descLayout,
					   descSet,
					   irradianceImage,
					   0); // Render to base mip level 0

	transitionCubemap(commandPool,
					  irradianceImage,
					  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
					  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					  1);

	vkDestroyDescriptorSetLayout(dev, descLayout, nullptr);
	vkDestroyDescriptorPool(dev, pool, nullptr);
}
