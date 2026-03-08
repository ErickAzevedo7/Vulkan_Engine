#pragma once

#include <cstdint>
#include <vulkan/vulkan_core.h>

namespace Renderer {
class VulkanDevice;
}

/**
 * VulkanIBL – Offline Image-Based Lighting setup.
 *
 * Loads an equirectangular HDR map and renders four offline cubemap passes:
 *   1. Equirect → environment cubemap  (512 × 512, R16G16B16A16_SFLOAT, mipmapped)
 *   2. Irradiance convolution          ( 32 ×  32, R16G16B16A16_SFLOAT)
 *   3. Pre-filtered specular env map   (128 × 128, R16G16B16A16_SFLOAT, 5 mip levels)
 *   4. BRDF integration LUT            (512 × 512, R16G16_SFLOAT, 2D texture)
 *
 * Results are bound to the PBR descriptor set for environment lighting.
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
	VkImageView getPrefilterImageView() const {
		return prefilterImageView;
	}
	VkSampler getPrefilterSampler() const {
		return prefilterSampler;
	}
	VkImageView getBrdfLutImageView() const {
		return brdfLutImageView;
	}
	VkSampler getBrdfLutSampler() const {
		return brdfLutSampler;
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

	// Pre-filtered specular environment cubemap (128x128, 5 mip levels)
	VkImage prefilterImage = VK_NULL_HANDLE;
	VkDeviceMemory prefilterMemory = VK_NULL_HANDLE;
	VkImageView prefilterImageView = VK_NULL_HANDLE;
	VkSampler prefilterSampler = VK_NULL_HANDLE;

	// BRDF integration LUT (512x512 RG16F 2D texture)
	VkImage brdfLutImage = VK_NULL_HANDLE;
	VkDeviceMemory brdfLutMemory = VK_NULL_HANDLE;
	VkImageView brdfLutImageView = VK_NULL_HANDLE;
	VkSampler brdfLutSampler = VK_NULL_HANDLE;

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
	void buildPrefilterMap(VkCommandPool commandPool);
	void buildBrdfLut(VkCommandPool commandPool);

	// Core helper that runs one of the two offline cube-capture render passes.
	void renderCubemapFaces(VkCommandPool commandPool,
							uint32_t size,
							const char* vertSpv,
							const char* fragSpv,
							VkDescriptorSetLayout descLayout,
							VkDescriptorSet descSet,
							VkImage dstImage,
							uint32_t mipLevel,
							float roughness = 0.0f);

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
