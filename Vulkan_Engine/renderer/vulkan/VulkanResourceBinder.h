#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../GraphicsBuffer.h"
#include "../GraphicsResourceBinder.h"
#include "renderer/RenderTypes.h"
#include "vulkan/vulkan_core.h"

namespace Renderer {

class GraphicsTexture; // Forward declaration

/**
 * Vulkan implementation of GraphicsResourceBinder.
 * Maps ResourceSet abstraction to Vulkan descriptor sets.
 */
class VulkanResourceBinder : public GraphicsResourceBinder {
public:
	VulkanResourceBinder() = default;
	~VulkanResourceBinder() override;

	/**
	 * Initialize the Vulkan resource binder.
	 * @param device Vulkan device
	 * @param bufferMgr Pointer to buffer manager for BufferHandle -> VkBuffer lookups
	 * @param textureMgr Pointer to texture manager for TextureHandle -> VkImageView lookups
	 */
	void initialize(VkDevice device, GraphicsBuffer* bufferMgr, GraphicsTexture* textureMgr);

	/**
	 * Shutdown and cleanup all resources.
	 */
	void shutdown();

	/**
	 * Create a descriptor pool (Vulkan-specific).
	 * This must be called before allocating any resource sets.
	 *
	 * @param maxSets Maximum number of descriptor sets
	 * @param poolSizes Pool size descriptors
	 */
	void createPool(uint32_t maxSets, const std::vector<VkDescriptorPoolSize>& poolSizes);

	// ========================================================================
	// GraphicsResourceBinder Interface Implementation
	// ========================================================================

	ResourceSetLayoutHandle createLayout(const ResourceSetLayoutDesc& desc) override;
	void destroyLayout(ResourceSetLayoutHandle layout) override;

	ResourceSetHandle allocateSet(ResourceSetLayoutHandle layout) override;
	void freeSet(ResourceSetHandle set) override;

	void updateBufferBindings(ResourceSetHandle set, const std::vector<ResourceBufferBinding>& bindings) override;

	void updateImageBindings(ResourceSetHandle set, const std::vector<ResourceImageBinding>& bindings) override;

	void updateSet(ResourceSetHandle set,
				   const std::vector<ResourceBufferBinding>& bufferBindings,
				   const std::vector<ResourceImageBinding>& imageBindings) override;

	void* getNativeHandle(ResourceSetHandle set) const override;

private:
	// Vulkan state
	VkDevice device = VK_NULL_HANDLE;
	VkDescriptorPool pool = VK_NULL_HANDLE;
	GraphicsBuffer* bufferManager = nullptr;
	GraphicsTexture* textureManager = nullptr;

	// Handle -> Vulkan resource maps
	std::unordered_map<uint64_t, VkDescriptorSet> sets;
	std::unordered_map<uint64_t, VkDescriptorSetLayout> layouts;
	std::unordered_map<uint64_t, ResourceSetLayoutDesc> layoutDescs; // Store for allocation

	// ID generators
	uint64_t nextSetId = 1;
	uint64_t nextLayoutId = 1;

	// Helper conversion functions
	VkDescriptorType toVulkanDescriptorType(ResourceType type) const;
	VkShaderStageFlags toVulkanShaderStages(ShaderStage stages) const;
};

} // namespace Renderer
