#include "VulkanBuffer.h"

#include <cstdint>
#include <cstring>
#include <stdexcept>

#include "renderer/RenderTypes.h"
#include "vulkan/vulkan_core.h"

namespace Renderer {

void VulkanBuffer::initialize(VkDevice dev, VkPhysicalDevice physDev) {
	device = dev;
	physicalDevice = physDev;
	nextHandleId = 1;
	buffers.clear();
}

void VulkanBuffer::shutdown() {
	// Destroy all remaining buffers
	for (auto& [id, resource] : buffers) {
		if (resource.mappedData) {
			vkUnmapMemory(device, resource.memory);
		}
		if (resource.buffer != VK_NULL_HANDLE) {
			vkDestroyBuffer(device, resource.buffer, nullptr);
		}
		if (resource.memory != VK_NULL_HANDLE) {
			vkFreeMemory(device, resource.memory, nullptr);
		}
	}
	buffers.clear();
}

BufferHandle VulkanBuffer::createBuffer(const BufferDesc& desc) {
	if (desc.size == 0) {
		return BufferHandle{}; // Invalid
	}

	VulkanBufferResource resource;
	resource.size = desc.size;
	resource.memoryType = desc.memory;

	// Create Vulkan buffer
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = desc.size;
	bufferInfo.usage = getVulkanBufferUsage(desc.usage);
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(device, &bufferInfo, nullptr, &resource.buffer) != VK_SUCCESS) {
		return BufferHandle{}; // Failed
	}

	// Allocate memory
	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, resource.buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, getVulkanMemoryProperties(desc.memory));

	if (vkAllocateMemory(device, &allocInfo, nullptr, &resource.memory) != VK_SUCCESS) {
		vkDestroyBuffer(device, resource.buffer, nullptr);
		return BufferHandle{}; // Failed
	}

	// Bind buffer to memory
	vkBindBufferMemory(device, resource.buffer, resource.memory, 0);

	// Persistent mapping for CPU-writable buffers
	if (desc.memory == MemoryType::CpuToGpu || desc.memory == MemoryType::GpuToCpu) {
		vkMapMemory(device, resource.memory, 0, desc.size, 0, &resource.mappedData);
	}

	// Generate handle and store resource
	BufferHandle handle;
	handle.id = nextHandleId++;
	buffers[handle.id] = resource;

	return handle;
}

void VulkanBuffer::destroyBuffer(BufferHandle handle) {
	auto it = buffers.find(handle.id);
	if (it == buffers.end()) {
		return; // Invalid handle
	}

	auto& resource = it->second;

	if (resource.mappedData) {
		vkUnmapMemory(device, resource.memory);
	}
	if (resource.buffer != VK_NULL_HANDLE) {
		vkDestroyBuffer(device, resource.buffer, nullptr);
	}
	if (resource.memory != VK_NULL_HANDLE) {
		vkFreeMemory(device, resource.memory, nullptr);
	}

	buffers.erase(it);
}

void VulkanBuffer::updateBuffer(BufferHandle handle, const void* data, size_t size, size_t offset) {
	auto it = buffers.find(handle.id);
	if (it == buffers.end() || !data) {
		return; // Invalid
	}

	auto& resource = it->second;

	if (offset + size > resource.size) {
		return; // Out of bounds
	}

	if (resource.mappedData) {
		// Use persistent mapping
		std::memcpy(static_cast<char*>(resource.mappedData) + offset, data, size);
	} else {
		// Temporary mapping
		void* mapped;
		vkMapMemory(device, resource.memory, offset, size, 0, &mapped);
		std::memcpy(mapped, data, size);
		vkUnmapMemory(device, resource.memory);
	}
}

void* VulkanBuffer::mapBuffer(BufferHandle handle) {
	auto it = buffers.find(handle.id);
	if (it == buffers.end()) {
		return nullptr;
	}

	auto& resource = it->second;

	if (resource.mappedData) {
		return resource.mappedData; // Already mapped
	}

	void* mapped = nullptr;
	vkMapMemory(device, resource.memory, 0, resource.size, 0, &mapped);
	return mapped;
}

void VulkanBuffer::unmapBuffer(BufferHandle handle) {
	auto it = buffers.find(handle.id);
	if (it == buffers.end()) {
		return;
	}

	auto& resource = it->second;

	// Don't unmap if persistently mapped
	if (!resource.mappedData) {
		vkUnmapMemory(device, resource.memory);
	}
}

size_t VulkanBuffer::getBufferSize(BufferHandle handle) const {
	auto it = buffers.find(handle.id);
	if (it == buffers.end()) {
		return 0;
	}
	return it->second.size;
}

VkBuffer VulkanBuffer::getVulkanBuffer(BufferHandle handle) const {
	auto it = buffers.find(handle.id);
	if (it == buffers.end()) {
		return VK_NULL_HANDLE;
	}
	return it->second.buffer;
}

// ============================================================================
// Helper Functions
// ============================================================================

uint32_t VulkanBuffer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("Failed to find suitable memory type!");
}

VkBufferUsageFlags VulkanBuffer::getVulkanBufferUsage(BufferUsage usage) {
	switch (usage) {
	case BufferUsage::Vertex:
		return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	case BufferUsage::Index:
		return VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	case BufferUsage::Uniform:
		return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	case BufferUsage::Storage:
		return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	case BufferUsage::Staging:
		return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	default:
		return 0;
	}
}

VkMemoryPropertyFlags VulkanBuffer::getVulkanMemoryProperties(MemoryType memoryType) {
	switch (memoryType) {
	case MemoryType::GpuOnly:
		return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	case MemoryType::CpuToGpu:
		return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	case MemoryType::GpuToCpu:
		return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	default:
		return 0;
	}
}

} // namespace Renderer
