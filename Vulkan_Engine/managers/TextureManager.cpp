#include "TextureManager.h"

// initialize static members
std::unordered_map<std::string, Texture> TextureManager::textures;

void TextureManager::loadDefaults() {
  Texture* texture =
      loadTexture("textures/texture.png", VulkanCore::getDevice(),
                  VulkanCore::getPhysicalDevice(), VulkanCore::getCommandPool(),
                  VulkanCore::getGraphicsQueue());
  createTextureImageView(texture);
  createTextureSampler(texture);
  textures["default"] = *texture;
}

Texture* TextureManager::loadTexture(const std::string& path,
                                     VkDevice device,
                                     VkPhysicalDevice physicalDevice,
                                     VkCommandPool commandPool,
                                     VkQueue graphicsQueue) {
  int texWidth, texHeight, texChannels;
  stbi_set_flip_vertically_on_load(true);

  stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels,
                              STBI_rgb_alpha);
  VkDeviceSize imageSize = texWidth * texHeight * 4;

  if (!pixels) {
    throw std::runtime_error("failed to load texture image!");
  }

  Texture* texture = new Texture();
  texture->width = static_cast<uint32_t>(texWidth);
  texture->height = static_cast<uint32_t>(texHeight);

  texture->mipLevels = static_cast<uint32_t>(std::floor(
                           std::log2(std::max(texWidth, texHeight)))) + 1;

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;

  Utils::createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
  memcpy(data, pixels, static_cast<size_t>(imageSize));
  vkUnmapMemory(device, stagingBufferMemory);

  stbi_image_free(pixels);

  Utils::createImage(
      texWidth, texHeight, texture->mipLevels, VK_SAMPLE_COUNT_1_BIT,
      VK_FORMAT_R8G8B8A8_SRGB,
      VK_IMAGE_TILING_OPTIMAL,
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
          VK_IMAGE_USAGE_SAMPLED_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, texture->image, texture->memory);

  Utils::transitionImageLayout(
	  texture->image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
	  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, texture->mipLevels, commandPool);
  Utils::copyBufferToImage(stagingBuffer, texture->image,
                           static_cast<uint32_t>(texWidth),
                           static_cast<uint32_t>(texHeight), commandPool);

  // transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating
  // mipmaps
  generateMipmaps(texture->image, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight,
                  texture->mipLevels);

  vkDestroyBuffer(device, stagingBuffer, nullptr);
  vkFreeMemory(device, stagingBufferMemory, nullptr);

  return texture;
}

void TextureManager::createTextureSampler(Texture* texture) {
  VkSamplerCreateInfo samplerInfo{};
  samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  samplerInfo.magFilter = VK_FILTER_LINEAR;
  samplerInfo.minFilter = VK_FILTER_LINEAR;
  samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  samplerInfo.anisotropyEnable = VK_TRUE;

  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(VulkanCore::getPhysicalDevice(), &properties);

  samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
  samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
  samplerInfo.unnormalizedCoordinates = VK_FALSE;
  samplerInfo.compareEnable = VK_FALSE;
  samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
  samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerInfo.mipLodBias = 0.0f;
  samplerInfo.minLod = 0.0f;
  samplerInfo.maxLod = static_cast<float>(texture->mipLevels);

  if (vkCreateSampler(VulkanCore::getDevice(), &samplerInfo, nullptr,
                      &texture->sampler) != VK_SUCCESS) {
    throw std::runtime_error("failed to create texture sampler!");
  }
}

void TextureManager::createTextureImageView(Texture* texture) {
  texture->imageView = Utils::createImageView(texture->image, VK_FORMAT_R8G8B8A8_SRGB,
                             VK_IMAGE_ASPECT_COLOR_BIT, texture->mipLevels);
}

Texture* TextureManager::getTexture(const std::string& name) {
  if (textures.find(name) != textures.end()) {
    return &textures[name];
  } else {
    throw std::runtime_error("Texture not found: " + name);
  }
}

void TextureManager::generateMipmaps(VkImage image,
                                     VkFormat imageFormat,
                                     int32_t texWidth,
                                     int32_t texHeight,
                                     uint32_t mipLevels) {
  // Check if image format supports linear blitting
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(VulkanCore::getPhysicalDevice(),
                                      imageFormat, &formatProperties);

  if (!(formatProperties.optimalTilingFeatures &
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
    throw std::runtime_error(
        "texture image format does not support linear blitting!");
  }

  VkCommandBuffer commandBuffer =
      Utils::beginSingleTimeCommands(VulkanCore::getCommandPool());

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image = image;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;
  barrier.subresourceRange.levelCount = 1;

  int32_t mipWidth = texWidth;
  int32_t mipHeight = texHeight;

  for (uint32_t i = 1; i < mipLevels; i++) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    VkImageBlit blit{};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
    blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel = i - 1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1,
                          mipHeight > 1 ? mipHeight / 2 : 1, 1};
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel = i;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount = 1;

    vkCmdBlitImage(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit,
                   VK_FILTER_LINEAR);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         0, nullptr, 1, &barrier);

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

  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  Utils::endSingleTimeCommands(VulkanCore::getCommandPool(), commandBuffer);
}

void TextureManager::cleanup() {
  for (auto& pair : textures) {
    vkDestroySampler(VulkanCore::getDevice(), pair.second.sampler, nullptr);
    vkDestroyImageView(VulkanCore::getDevice(), pair.second.imageView, nullptr);
    vkDestroyImage(VulkanCore::getDevice(), pair.second.image, nullptr);
    vkFreeMemory(VulkanCore::getDevice(), pair.second.memory, nullptr);
  }
  textures.clear();
}
