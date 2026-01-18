#pragma once
#include <stb_image.h>
#include <string>
#include <unordered_map>
#include <filesystem>

#include "core/utils/Utils.h"
#include "core/vulkancore.h"

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
  static void loadDefaults();

	// Load all textures found under assets directory at startup
	static void loadAllFromAssets(const std::string& assetsRoot);

  static Texture* loadTexture(const std::string& path,
                       VkDevice device,
                       VkPhysicalDevice physicalDevice,
                       VkCommandPool commandPool,
                       VkQueue graphicsQueue);

	static void createTextureSampler(Texture* texture);

  static void createThumbnailSampler(ThumbnailTexture* texture);

  static void createTextureImageView(Texture* texture);

  static Texture* getTexture(const std::string& name);
	static const ThumbnailTexture* getThumbnail(const std::string& key);

  static const std::unordered_map<std::string, Texture>& getAllTextures();

	// Register an existing texture under a logical name in the manager map
	static void registerTexture(const std::string& name, const Texture& texture);

  static void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

  static void cleanup();

 private:
  static std::unordered_map<std::string, Texture> textures;
	static std::unordered_map<std::string, ThumbnailTexture> thumbnails;

	// Create (or reuse existing) downscaled thumbnail texture from a full Texture
	static ThumbnailTexture& createThumbnail(const std::string& key, const Texture& sourceTexture);
};
