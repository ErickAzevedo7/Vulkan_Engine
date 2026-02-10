#include "TextureManager.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stb_image.h>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "core/utils/Utils.h"
#include "core/vulkancore.h"
#include "vulkan/vulkan_core.h"

TextureManager::TextureManager() {
	// Initialization deferred to loadDefaults() called by ResourceContext
}

TextureManager::~TextureManager() {
	// Destructor - automatic cleanup (RAII)
	for (auto& pair : textures) {
		vkDestroySampler(VulkanCore::getDevice(), pair.second.sampler, nullptr);
		vkDestroyImageView(VulkanCore::getDevice(), pair.second.imageView, nullptr);
		vkDestroyImage(VulkanCore::getDevice(), pair.second.image, nullptr);
		vkFreeMemory(VulkanCore::getDevice(), pair.second.memory, nullptr);
	}
	textures.clear();

	for (auto& pair : thumbnails) {
		if (pair.second.sampler != VK_NULL_HANDLE) {
			vkDestroySampler(VulkanCore::getDevice(), pair.second.sampler, nullptr);
		}
		if (pair.second.imageView != VK_NULL_HANDLE) {
			vkDestroyImageView(VulkanCore::getDevice(), pair.second.imageView, nullptr);
		}
		if (pair.second.image != VK_NULL_HANDLE) {
			vkDestroyImage(VulkanCore::getDevice(), pair.second.image, nullptr);
		}
		if (pair.second.memory != VK_NULL_HANDLE) {
			vkFreeMemory(VulkanCore::getDevice(), pair.second.memory, nullptr);
		}
	}
	thumbnails.clear();
}

void TextureManager::loadDefaults() {
	// load icons textures - UI icons should NOT be flipped
	Texture* texture = loadTexture("ui/icons/defaultFile.png",
								   VulkanCore::getDevice(),
								   VulkanCore::getPhysicalDevice(),
								   VulkanCore::getCommandPool(),
								   VulkanCore::getGraphicsQueue(),
								   false);
	createTextureImageView(texture);
	createTextureSampler(texture);
	textures["defaultFile"] = *texture;

	texture = loadTexture("ui/icons/folder.png",
						  VulkanCore::getDevice(),
						  VulkanCore::getPhysicalDevice(),
						  VulkanCore::getCommandPool(),
						  VulkanCore::getGraphicsQueue(),
						  false);
	createTextureImageView(texture);
	createTextureSampler(texture);
	textures["folder"] = *texture;

	// load default object texture - 3D textures SHOULD be flipped
	texture = loadTexture(kDefaultTextureKey,
						  VulkanCore::getDevice(),
						  VulkanCore::getPhysicalDevice(),
						  VulkanCore::getCommandPool(),
						  VulkanCore::getGraphicsQueue(),
						  true);
	createTextureImageView(texture);
	createTextureSampler(texture);
	textures[kDefaultTextureKey] = *texture;
}

void TextureManager::loadAllFromAssets(const std::string& assetsRoot) {
	namespace fs = std::filesystem;

	fs::path root(assetsRoot);
	if (!fs::exists(root) || !fs::is_directory(root)) {
		return;
	}

	for (auto const& entry : fs::recursive_directory_iterator(root)) {
		if (!entry.is_regular_file()) {
			continue;
		}

		fs::path p = entry.path();
		std::string ext = p.extension().string();
		for (char& c : ext) {
			c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
		}

		// supported texture extensions
		if (ext != ".png" && ext != ".jpg" && ext != ".jpeg" && ext != ".tga" && ext != ".bmp" && ext != ".hdr") {
			continue;
		}

		const std::string fullPath = p.string();
		const std::string normFullPath = std::filesystem::path(fullPath).generic_string();
		if (textures.find(normFullPath) != textures.end()) {
			continue;
		}

		// Check if it's a UI icon (heuristic: path contains "ui/icons" or similar?)
		// For now, assume everything in assets/ is a 3D texture, EXCEPT if user organizes otherwise.
		// Detailed check: if path contains "ui/icons", don't flip.
		bool flip = true;
		if (normFullPath.find("ui/icons/") != std::string::npos) {
			flip = false;
		}

		Texture* texture = loadTexture(fullPath,
									   VulkanCore::getDevice(),
									   VulkanCore::getPhysicalDevice(),
									   VulkanCore::getCommandPool(),
									   VulkanCore::getGraphicsQueue(),
									   flip);
		createTextureImageView(texture);
		createTextureSampler(texture);
		textures[normFullPath] = *texture;

		// Also create a small thumbnail texture for UI from this source
		createThumbnail(normFullPath, *texture);
	}
}

const ThumbnailTexture* TextureManager::getThumbnail(const std::string& key) {
	std::string k;

	k = std::filesystem::path(key).generic_string();

	auto it = thumbnails.find(k);
	if (it != thumbnails.end()) {
		return &it->second;
	}
	return nullptr;
}

ThumbnailTexture& TextureManager::createThumbnail(const std::string& key, const Texture& sourceTexture) {
	// Fixed thumbnail size (square) - adjust as needed
	const uint32_t thumbSize = 96;
	ThumbnailTexture thumb;
	thumb.width = thumbSize;
	thumb.height = thumbSize;

	VkDevice device = VulkanCore::getDevice();
	VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

	Utils::createImage(static_cast<int32_t>(thumb.width),
					   static_cast<int32_t>(thumb.height),
					   1,
					   VK_SAMPLE_COUNT_1_BIT,
					   format,
					   VK_IMAGE_TILING_OPTIMAL,
					   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					   thumb.image,
					   thumb.memory);

	// Copy/downscale from sourceTexture.image into thumb.image using blit
	VkCommandBuffer cmd = Utils::beginSingleTimeCommands(VulkanCore::getCommandPool());

	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	// Transition thumb image to TRANSFER_DST_OPTIMAL
	barrier.image = thumb.image;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	vkCmdPipelineBarrier(
		cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	// Ensure source is in TRANSFER_SRC_OPTIMAL
	VkImageMemoryBarrier srcBarrier{};
	srcBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	srcBarrier.image = sourceTexture.image;
	srcBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	srcBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	srcBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	srcBarrier.subresourceRange.baseArrayLayer = 0;
	srcBarrier.subresourceRange.layerCount = 1;
	srcBarrier.subresourceRange.levelCount = 1;
	srcBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	srcBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	srcBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
	srcBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

	vkCmdPipelineBarrier(cmd,
						 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						 VK_PIPELINE_STAGE_TRANSFER_BIT,
						 0,
						 0,
						 nullptr,
						 0,
						 nullptr,
						 1,
						 &srcBarrier);

	VkImageBlit blit{};
	blit.srcOffsets[0] = {0, 0, 0};
	blit.srcOffsets[1] = {static_cast<int32_t>(sourceTexture.width), static_cast<int32_t>(sourceTexture.height), 1};
	blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.srcSubresource.mipLevel = 0;
	blit.srcSubresource.baseArrayLayer = 0;
	blit.srcSubresource.layerCount = 1;
	blit.dstOffsets[0] = {0, 0, 0};
	blit.dstOffsets[1] = {static_cast<int32_t>(thumb.width), static_cast<int32_t>(thumb.height), 1};
	blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blit.dstSubresource.mipLevel = 0;
	blit.dstSubresource.baseArrayLayer = 0;
	blit.dstSubresource.layerCount = 1;

	vkCmdBlitImage(cmd,
				   sourceTexture.image,
				   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				   thumb.image,
				   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				   1,
				   &blit,
				   VK_FILTER_LINEAR);

	// Transition images back: source to SHADER_READ_ONLY_OPTIMAL, thumb to SHADER_READ_ONLY_OPTIMAL
	srcBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	srcBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	srcBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	srcBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	VkImageMemoryBarrier barriers[2] = {srcBarrier, barrier};
	vkCmdPipelineBarrier(cmd,
						 VK_PIPELINE_STAGE_TRANSFER_BIT,
						 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
						 0,
						 0,
						 nullptr,
						 0,
						 nullptr,
						 2,
						 barriers);

	Utils::endSingleTimeCommands(VulkanCore::getCommandPool(), cmd);

	// Create image view and sampler for thumbnail
	thumb.imageView = Utils::createImageView(thumb.image, format, VK_IMAGE_ASPECT_COLOR_BIT, 1);

	// Small, no-mipmap sampler tuned for UI
	TextureManager::createThumbnailSampler(&thumb);

	std::string k;

	k = std::filesystem::path(key).generic_string();

	thumbnails[k] = thumb;

	return thumbnails[k];
}

Texture* TextureManager::loadTexture(const std::string& path,
									 VkDevice device,
									 VkPhysicalDevice physicalDevice,
									 VkCommandPool commandPool,
									 VkQueue graphicsQueue,
									 bool flipV) {
	int texWidth, texHeight, texChannels;

	stbi_set_flip_vertically_on_load(flipV);
	stbi_uc* pixels = stbi_load(path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels) {
		throw std::runtime_error("failed to load texture image!");
	}

	Texture* texture = new Texture();
	texture->width = static_cast<uint32_t>(texWidth);
	texture->height = static_cast<uint32_t>(texHeight);

	texture->mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;

	Utils::createBuffer(imageSize,
						VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
						stagingBuffer,
						stagingBufferMemory);

	void* data;
	vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(device, stagingBufferMemory);

	stbi_image_free(pixels);

	Utils::createImage(texWidth,
					   texHeight,
					   texture->mipLevels,
					   VK_SAMPLE_COUNT_1_BIT,
					   VK_FORMAT_R8G8B8A8_SRGB,
					   VK_IMAGE_TILING_OPTIMAL,
					   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
					   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
					   texture->image,
					   texture->memory);

	Utils::transitionImageLayout(texture->image,
								 VK_FORMAT_R8G8B8A8_SRGB,
								 VK_IMAGE_LAYOUT_UNDEFINED,
								 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
								 texture->mipLevels,
								 commandPool);
	Utils::copyBufferToImage(
		stagingBuffer, texture->image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), commandPool);

	// transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating
	// mipmaps
	generateMipmaps(texture->image, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, texture->mipLevels);

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

	if (vkCreateSampler(VulkanCore::getDevice(), &samplerInfo, nullptr, &texture->sampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}
}

void TextureManager::createThumbnailSampler(ThumbnailTexture* texture) {
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.anisotropyEnable = VK_FALSE;
	samplerInfo.maxAnisotropy = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	if (vkCreateSampler(VulkanCore::getDevice(), &samplerInfo, nullptr, &texture->sampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create thumbnail sampler!");
	}
}

void TextureManager::createTextureImageView(Texture* texture) {
	texture->imageView =
		Utils::createImageView(texture->image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, texture->mipLevels);
}

Texture* TextureManager::getTexture(const std::string& path) {
	// Normalize the file path key to a consistent form for storage/lookups.
	std::string key;

	key = std::filesystem::path(path).generic_string();

	auto it = textures.find(key);
	if (it != textures.end()) {
		return &it->second;
	}
	std::cerr << "TextureManager::getTexture: texture not found for path='" << key << "' (original='" << path << "')"
			  << std::endl;
	throw std::runtime_error("Texture not found: " + key);
}

const std::unordered_map<std::string, Texture>& TextureManager::getAllTextures() {
	return textures;
}

void TextureManager::registerTexture(const std::string& path, const Texture& texture) {
	std::string key;

	key = std::filesystem::path(path).generic_string();

	textures[key] = texture;
	std::cerr << "TextureManager::registerTexture: registered texture path='" << key << "'" << std::endl;
}

void TextureManager::generateMipmaps(
	VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels) {
	// Check if image format supports linear blitting
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(VulkanCore::getPhysicalDevice(), imageFormat, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("texture image format does not support linear blitting!");
	}

	VkCommandBuffer commandBuffer = Utils::beginSingleTimeCommands(VulkanCore::getCommandPool());

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

		VkImageBlit blit{};
		blit.srcOffsets[0] = {0, 0, 0};
		blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = {0, 0, 0};
		blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(commandBuffer,
					   image,
					   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					   image,
					   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					   1,
					   &blit,
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

	Utils::endSingleTimeCommands(VulkanCore::getCommandPool(), commandBuffer);
}
