#pragma once
#include <vulkan/vulkan.h>
#include <stdexcept>

namespace Utils {
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
}  // namespace Utils