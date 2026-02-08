#pragma once
#include "vulkan/vulkan_core.h"

class Vertices {
public:
	static void
	createVertexBuffer(VkCommandPool commandPool, VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory);

	static void createIndexBuffer(VkCommandPool commandPool, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory);
};
