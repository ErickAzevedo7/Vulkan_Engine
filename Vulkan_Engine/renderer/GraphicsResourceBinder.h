#pragma once

#include <vector>

#include "RenderTypes.h"


namespace Renderer {

/**
 * Abstract interface for binding resources (buffers, textures) to shaders.
 *
 * The ResourceSet abstraction provides a unified way to bind GPU resources across different APIs:
 * - Vulkan: Maps to descriptor sets, descriptor pools, and descriptor set layouts
 * - DirectX 12: Maps to descriptor tables, descriptor heaps, and root signatures
 * - Metal: Maps to argument buffers (or individual setBuffer/setTexture calls)
 * - OpenGL: Maps to cached binding state, applied via glBindBuffer/glBindTexture
 *
 * This interface hides API-specific details while providing the flexibility needed
 * for different binding models.
 */
class GraphicsResourceBinder {
public:
	virtual ~GraphicsResourceBinder() = default;

	// ========================================================================
	// Layout Management
	// ========================================================================

	/**
	 * Create a resource set layout that defines what bindings are in a set.
	 *
	 * API Mappings:
	 * - Vulkan: VkDescriptorSetLayout
	 * - DX12: Part of root signature definition
	 * - Metal: Argument buffer layout metadata
	 * - OpenGL: Cached layout info for validation
	 *
	 * @param desc Layout description with bindings
	 * @return Handle to the created layout
	 */
	virtual ResourceSetLayoutHandle createLayout(const ResourceSetLayoutDesc& desc) = 0;

	/**
	 * Destroy a resource set layout.
	 * @param layout Layout to destroy
	 */
	virtual void destroyLayout(ResourceSetLayoutHandle layout) = 0;

	// ========================================================================
	// Resource Set Allocation
	// ========================================================================

	/**
	 * Allocate a resource set matching the given layout.
	 *
	 * API Mappings:
	 * - Vulkan: Allocate from descriptor pool
	 * - DX12: Allocate from descriptor heap
	 * - Metal: Create argument buffer
	 * - OpenGL: Allocate internal binding cache
	 *
	 * @param layout Layout describing the set structure
	 * @return Handle to the allocated resource set
	 */
	virtual ResourceSetHandle allocateSet(ResourceSetLayoutHandle layout) = 0;

	/**
	 * Free a resource set.
	 * @param set Resource set to free
	 */
	virtual void freeSet(ResourceSetHandle set) = 0;

	// ========================================================================
	// Resource Set Updates
	// ========================================================================

	/**
	 * Update a resource set with buffer bindings.
	 * This associates actual GPU buffers with binding slots defined in the layout.
	 *
	 * @param set Resource set to update
	 * @param bindings Buffer bindings to apply
	 */
	virtual void updateBufferBindings(ResourceSetHandle set, const std::vector<ResourceBufferBinding>& bindings) = 0;

	/**
	 * Update a resource set with image/texture bindings.
	 * This associates textures and samplers with binding slots.
	 *
	 * @param set Resource set to update
	 * @param bindings Image bindings to apply
	 */
	virtual void updateImageBindings(ResourceSetHandle set, const std::vector<ResourceImageBinding>& bindings) = 0;

	/**
	 * Batch update with both buffer and image bindings.
	 * More efficient than separate calls for APIs that support batch updates.
	 *
	 * @param set Resource set to update
	 * @param bufferBindings Buffer bindings to apply
	 * @param imageBindings Image bindings to apply
	 */
	virtual void updateSet(ResourceSetHandle set,
						   const std::vector<ResourceBufferBinding>& bufferBindings,
						   const std::vector<ResourceImageBinding>& imageBindings) = 0;

	// ========================================================================
	// Backend-Specific Access (Temporary)
	// ========================================================================

	/**
	 * Get the native API handle for a resource set.
	 * Used during rendering for binding until we have command buffer abstraction.
	 *
	 * Returns:
	 * - Vulkan: VkDescriptorSet*
	 * - DX12: D3D12_GPU_DESCRIPTOR_HANDLE*
	 * - Metal: id<MTLArgumentEncoder>
	 * - OpenGL: Internal binding state pointer
	 *
	 * TODO: Remove once we have command buffer abstraction
	 *
	 * @param set Resource set handle
	 * @return Pointer to native API handle
	 */
	virtual void* getNativeHandle(ResourceSetHandle set) const = 0;
};

} // namespace Renderer
