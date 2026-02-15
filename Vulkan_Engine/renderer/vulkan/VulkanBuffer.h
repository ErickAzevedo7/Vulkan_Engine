#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vulkan/vulkan.h>

#include "../GraphicsBuffer.h"
#include "renderer/RenderTypes.h"
#include "vulkan/vulkan_core.h"

namespace Renderer {

/**
 * @brief Vulkan-specific buffer resource
 * Stores VkBuffer, VkDeviceMemory, and metadata
 */
struct VulkanBufferResource {
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	size_t size = 0;
	MemoryType memoryType = MemoryType::GpuOnly;
	void* mappedData = nullptr; // Non-null if persistently mapped
};

/**
 * @brief Vulkan implementation of GraphicsBuffer interface
 */
class VulkanBuffer : public GraphicsBuffer {
public:
	/**
	 * @brief Initialize the Vulkan buffer manager
	 * @param device Vulkan logical device
	 * @param physicalDevice Vulkan physical device (for memory properties)
	 */
	void initialize(VkDevice device, VkPhysicalDevice physicalDevice);

	/**
	 * @brief Shutdown and cleanup all buffers
	 */
	void shutdown();

	// GraphicsBuffer interface implementation
	BufferHandle createBuffer(const BufferDesc& desc) override;
	void destroyBuffer(BufferHandle handle) override;
	void updateBuffer(BufferHandle handle, const void* data, size_t size, size_t offset = 0) override;
	void* mapBuffer(BufferHandle handle) override;
	void unmapBuffer(BufferHandle handle) override;
	size_t getBufferSize(BufferHandle handle) const override;

	/**
	 * @brief Get the underlying Vulkan buffer (for interop with existing code)
	 * @param handle Buffer handle
	 * @return VkBuffer, or VK_NULL_HANDLE if invalid
	 */
	VkBuffer getVulkanBuffer(BufferHandle handle) const;

private:
	VkDevice device = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

	// Handle generation
	uint64_t nextHandleId = 1;

	// Resource storage
	std::unordered_map<uint64_t, VulkanBufferResource> buffers;

	// Helper functions
	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	VkBufferUsageFlags getVulkanBufferUsage(BufferUsage usage);
	VkMemoryPropertyFlags getVulkanMemoryProperties(MemoryType memoryType);
};

} // namespace Renderer
