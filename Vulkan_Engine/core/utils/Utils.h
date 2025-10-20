#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>

namespace Utils {
bool hasStencilComponent(VkFormat format);

void copyBuffer(VkCommandPool commandPool,
                VkBuffer srcBuffer,
                VkBuffer dstBuffer,
                VkDeviceSize size);

void createBuffer(VkDeviceSize size,
                  VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties,
                  VkBuffer& buffer,
                  VkDeviceMemory& bufferMemory);

VkCommandBuffer beginSingleTimeCommands(VkCommandPool commandPool);

void endSingleTimeCommands(VkCommandPool commandPool,
                           VkCommandBuffer commandBuffer);

uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

void createImage(uint32_t width,
                 uint32_t height,
                 uint32_t mipLevels,
                 VkSampleCountFlagBits numSamples,
                 VkFormat format,
                 VkImageTiling tiling,
                 VkImageUsageFlags usage,
                 VkMemoryPropertyFlags properties,
                 VkImage& image,
                 VkDeviceMemory& imageMemory);

void transitionImageLayout(VkImage image,
                           VkFormat format,
                           VkImageLayout oldLayout,
                           VkImageLayout newLayout,
                           uint32_t mipLevels,
                           VkCommandPool commandPool);
void copyBufferToImage(VkBuffer buffer,
                       VkImage image,
                       uint32_t width,
                       uint32_t height,
                       VkCommandPool commandPool);
}  // namespace Utils