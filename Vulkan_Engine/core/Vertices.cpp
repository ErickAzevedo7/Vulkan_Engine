#include "Vertices.h"

#include "core/utils/Utils.h"
#include "core/vulkancore.h"
#include "managers/MeshManager.h" // Must come before vulkancore.h to get full Vertex definition
#include "vulkan/vulkan_core.h"

#include <cstring>

void Vertices::createVertexBuffer(VkCommandPool commandPool,
								  VkBuffer& vertexBuffer,
								  VkDeviceMemory& vertexBufferMemory) {
	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	Utils::createBuffer(bufferSize,
						VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
						stagingBuffer,
						stagingBufferMemory);

	void* data;
	vkMapMemory(VulkanCore::getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, vertices.data(), (size_t)bufferSize);
	vkUnmapMemory(VulkanCore::getDevice(), stagingBufferMemory);

	Utils::createBuffer(bufferSize,
						VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
						vertexBuffer,
						vertexBufferMemory);

	Utils::copyBuffer(commandPool, stagingBuffer, vertexBuffer, bufferSize);

	vkDestroyBuffer(VulkanCore::getDevice(), stagingBuffer, nullptr);
	vkFreeMemory(VulkanCore::getDevice(), stagingBufferMemory, nullptr);
}

void Vertices::createIndexBuffer(VkCommandPool commandPool, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory) {
	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	Utils::createBuffer(bufferSize,
						VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
						stagingBuffer,
						stagingBufferMemory);

	void* data;
	vkMapMemory(VulkanCore::getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
	memcpy(data, indices.data(), (size_t)bufferSize);
	vkUnmapMemory(VulkanCore::getDevice(), stagingBufferMemory);

	Utils::createBuffer(bufferSize,
						VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
						VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
						indexBuffer,
						indexBufferMemory);

	Utils::copyBuffer(commandPool, stagingBuffer, indexBuffer, bufferSize);

	vkDestroyBuffer(VulkanCore::getDevice(), stagingBuffer, nullptr);
	vkFreeMemory(VulkanCore::getDevice(), stagingBufferMemory, nullptr);
}