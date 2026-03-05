#pragma once

#include <cstdint>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "renderer/Hdr.h"


namespace Renderer {

class VulkanDevice;

class VulkanHdr : public Hdr {
public:
	VulkanHdr() = default;
	~VulkanHdr() override = default;

	void init(void* device, void* hdrResolveImageView, uint32_t width, uint32_t height) override;

	void recordHdrCommandBuffer(uint32_t currentFrame, uint32_t imageIndex, float exposure) override;

	void recreateHdr(void* hdrResolveImageView, uint32_t width, uint32_t height) override;

	void cleanup() override;

	void* getCommandBuffer(uint32_t currentFrame) const override {
		return hdrCommandBuffers[currentFrame];
	}

	void* getLdrImageView(uint32_t index) const override {
		return ldrImageViews[index];
	}

	uint32_t getLdrImageViewCount() const override {
		return static_cast<uint32_t>(ldrImageViews.size());
	}

private:
	Renderer::VulkanDevice* vulkanDevice = nullptr;

	VkExtent2D viewportExtent = {0, 0};
	VkImageView hdrResolveImageView = VK_NULL_HANDLE;

	std::vector<VkImage> ldrImages;
	std::vector<VkDeviceMemory> ldrImageMemory;
	std::vector<VkImageView> ldrImageViews;

	std::vector<VkFramebuffer> hdrFramebuffers;
	VkRenderPass hdrRenderPass = VK_NULL_HANDLE;
	VkPipelineLayout hdrPipelineLayout = VK_NULL_HANDLE;
	VkPipeline hdrPipeline = VK_NULL_HANDLE;

	VkDescriptorSetLayout hdrDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool hdrDescriptorPool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> hdrDescriptorSets;
	VkSampler hdrSampler = VK_NULL_HANDLE;

	std::vector<VkCommandBuffer> hdrCommandBuffers;

	void createHdrRenderPass();
	void createLdrImages();
	void createHdrFramebuffers();
	void createHdrSampler();
	void createDescriptorPool();
	void createHdrDescriptorSets();
	void createGraphicsPipeline();
	void createCommandBuffers();
	void cleanupFramebuffers();
};

} // namespace Renderer
