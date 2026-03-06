#pragma once

#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace Renderer {
class VulkanDevice;
}

/**
 * VulkanIBL – Offline Image-Based Lighting setup.
 *
 * Loads an equirectangular HDR map and renders two offline cubemap passes:
 *   1. Equirect → environment cubemap  (512 × 512, R16G16B16A16_SFLOAT)
 *   2. Irradiance convolution          ( 32 ×  32, R16G16B16A16_SFLOAT)
 *
 * The irradiance cubemap view/sampler can then be bound to the PBR descriptor
 * set so shader.frag can sample it for the ambient diffuse term.
 */
class VulkanIBL {
public:
	VulkanIBL() = default;
	~VulkanIBL() = default;

	/**
	 * @param device        Vulkan device wrapper
	 * @param commandPool   Command pool for one-shot commands
	 * @param hdrPath       Path to the equirectangular .hdr file
	 */
	void init(Renderer::VulkanDevice* device, VkCommandPool commandPool, const char* hdrPath);
	void cleanup();

	// Accessors used by MaterialManager to write descriptor set binding 6
	VkImageView getIrradianceImageView() const {
		return irradianceImageView;
	}
	VkSampler getIrradianceSampler() const {
		return cubemapSampler;
	}
	VkImageView getEnvCubemapImageView() const {
		return envCubemapImageView;
	}
	VkSampler getEnvCubemapSampler() const {
		return cubemapSampler;
	}

private:
	Renderer::VulkanDevice* vulkanDevice = nullptr;

	// Environment cubemap (512 × 512), built from the HDR equirect
	VkImage envCubemapImage = VK_NULL_HANDLE;
	VkDeviceMemory envCubemapMemory = VK_NULL_HANDLE;
	VkImageView envCubemapImageView = VK_NULL_HANDLE;

	// Irradiance cubemap (32 × 32), convolved from the env cubemap
	VkImage irradianceImage = VK_NULL_HANDLE;
	VkDeviceMemory irradianceMemory = VK_NULL_HANDLE;
	VkImageView irradianceImageView = VK_NULL_HANDLE;

	// Shared linear-clamp-to-edge sampler for both cubemaps
	VkSampler cubemapSampler = VK_NULL_HANDLE;

	// Temporary 2-D HDR image used only during equirect→cubemap pass
	VkImage hdrImage = VK_NULL_HANDLE;
	VkDeviceMemory hdrMemory = VK_NULL_HANDLE;
	VkImageView hdrImageView = VK_NULL_HANDLE;
	VkSampler hdrSampler = VK_NULL_HANDLE;

	// -----------------------------------------------------------------------
	void createCubemapSampler();
	void loadEquirect(VkCommandPool commandPool, const char* hdrPath);
	void buildEnvCubemap(VkCommandPool commandPool);
	void buildIrradianceMap(VkCommandPool commandPool);

	// Core helper that runs one of the two offline cube-capture render passes.
	void renderCubemapFaces(VkCommandPool commandPool,
							uint32_t size,
							const char* vertSpv,
							const char* fragSpv,
							VkDescriptorSetLayout descLayout,
							VkDescriptorSet descSet,
							VkImage dstImage,
							uint32_t mipLevel);

	// Helpers to create staging texture + image
	void createCubemapImage(uint32_t size, VkImage& img, VkDeviceMemory& mem, VkImageView& view, uint32_t mipLevels);
	void transitionCubemap(
		VkCommandPool commandPool, VkImage img, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

	void generateMipmaps(VkCommandPool commandPool,
						 VkImage image,
						 VkFormat imageFormat,
						 int32_t texWidth,
						 int32_t texHeight,
						 uint32_t mipLevels,
						 uint32_t layerCount);
};
