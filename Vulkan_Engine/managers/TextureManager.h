#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

#include "vulkan/vulkan_core.h"

struct Texture {
	VkImage image;
	VkDeviceMemory memory;
	VkImageView imageView;
	VkSampler sampler;
	uint32_t width;
	uint32_t height;
	uint32_t mipLevels;
};

struct ThumbnailTexture {
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	VkSampler sampler = VK_NULL_HANDLE;
	uint32_t width = 0;
	uint32_t height = 0;
};

class TextureManager {
public:
	TextureManager();
	~TextureManager();

	void loadDefaults();
	const std::string kDefaultTextureKey = "common/texture/default.png";

	// Load all textures found under assets directory at startup
	void loadAllFromAssets(const std::string& assetsRoot);

	Texture* loadTexture(const std::string& path,
						 VkDevice device,
						 VkPhysicalDevice physicalDevice,
						 VkCommandPool commandPool,
						 VkQueue graphicsQueue,
						 bool flipV = true);

	void createTextureSampler(Texture* texture);

	void createThumbnailSampler(ThumbnailTexture* texture);

	void createTextureImageView(Texture* texture);

	Texture* getTexture(const std::string& path);
	const ThumbnailTexture* getThumbnail(const std::string& key);

	const std::unordered_map<std::string, Texture>& getAllTextures();

	// Register an existing texture under its file path key in the manager map
	void registerTexture(const std::string& path, const Texture& texture);

	void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

private:
	std::unordered_map<std::string, Texture> textures;
	std::unordered_map<std::string, ThumbnailTexture> thumbnails;

	// Create (or reuse existing) downscaled thumbnail texture from a full Texture
	ThumbnailTexture& createThumbnail(const std::string& key, const Texture& sourceTexture);
};
