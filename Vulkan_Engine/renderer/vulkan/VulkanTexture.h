#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>

#include "renderer/GraphicsTexture.h"
#include "renderer/RenderTypes.h"
#include "vulkan/vulkan_core.h"

namespace Renderer {

struct VulkanTextureData {
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	// We might store format/extent here if needed
	uint32_t width = 0;
	uint32_t height = 0;
};

class VulkanTexture : public GraphicsTexture {
public:
	VulkanTexture();
	~VulkanTexture() override;

	void initialize(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue);
	void shutdown();

	// specific getter for Binder
	VkImageView getImageView(TextureHandle handle) const;
	VkSampler getSampler(SamplerHandle handle) const;

	// Interface implementation
	TextureHandle createTexture(const TextureDesc& desc, const void* initialData = nullptr) override;
	SamplerHandle createSampler(const SamplerDesc& desc) override;
	void destroyTexture(TextureHandle handle) override;
	void destroySampler(SamplerHandle handle) override;
	TextureHandle createThumbnail(TextureHandle source, uint32_t width, uint32_t height) override;

private:
	VkDevice device = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkQueue graphicsQueue = VK_NULL_HANDLE;

	std::unordered_map<uint64_t, VulkanTextureData> textures;
	std::unordered_map<uint64_t, VkSampler> samplers;

	uint64_t nextTextureId = 1;
	uint64_t nextSamplerId = 1;
	mutable std::mutex resourceMutex;

	// Helpers
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	void createImage(uint32_t width,
					 uint32_t height,
					 uint32_t mipLevels,
					 VkFormat format,
					 VkImageTiling tiling,
					 VkImageUsageFlags usage,
					 VkMemoryPropertyFlags properties,
					 VkImage& image,
					 VkDeviceMemory& imageMemory);
	void transitionImageLayout(
		VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);
	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
	void generateMipmaps(VkImage image, VkFormat format, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);
	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
};

} // namespace Renderer
