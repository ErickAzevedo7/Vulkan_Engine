#pragma once
#include <stb_image.h>
#include <string>
#include <unordered_map>

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

class TextureManager {
 public:
  static void loadDefaults();

  static Texture* loadTexture(const std::string& path,
                       VkDevice device,
                       VkPhysicalDevice physicalDevice,
                       VkCommandPool commandPool,
                       VkQueue graphicsQueue);

	static void createTextureSampler(Texture* texture);

  static void createTextureImageView(Texture* texture);

  static Texture* getTexture(const std::string& name);

  static const std::unordered_map<std::string, Texture>& getAllTextures();

  static void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

  static void cleanup();

 private:
  static std::unordered_map<std::string, Texture> textures;
};
