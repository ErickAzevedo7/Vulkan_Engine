#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "vulkan/vulkan_core.h"


namespace Utils {
bool hasStencilComponent(VkFormat format);

std::vector<char> readFile(const std::string& filename);

void copyBuffer(VkCommandPool commandPool, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

void createBuffer(VkDeviceSize size,
				  VkBufferUsageFlags usage,
				  VkMemoryPropertyFlags properties,
				  VkBuffer& buffer,
				  VkDeviceMemory& bufferMemory);

VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool);

void endSingleTimeCommands(VkCommandPool commandPool, VkCommandBuffer commandBuffer);

uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

void createShaderModule(const std::vector<char>& code, VkShaderModule& shaderModule);

void createImage(uint32_t width,
				 uint32_t height,
				 uint32_t mipLevels,
				 VkSampleCountFlagBits numSamples,
				 VkFormat format,
				 VkImageTiling tiling,
				 VkImageUsageFlags usage,
				 VkMemoryPropertyFlags properties,
				 VkImage& image,
				 VkDeviceMemory& imageMemory,
				 uint32_t arrayLayers = 1,
				 VkImageCreateFlags flags = 0);

void transitionImageLayout(VkImage image,
						   VkFormat format,
						   VkImageLayout oldLayout,
						   VkImageLayout newLayout,
						   uint32_t mipLevels,
						   VkCommandPool commandPool,
						   uint32_t layerCount = 1);
void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkCommandPool commandPool);
VkImageView createImageView(VkImage image,
							VkFormat format,
							VkImageAspectFlags aspectFlags,
							uint32_t mipLevels,
							VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D,
							uint32_t layerCount = 1);
} // namespace Utils