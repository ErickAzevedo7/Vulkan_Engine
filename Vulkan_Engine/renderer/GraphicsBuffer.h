#pragma once

#include <cstddef>

#include "RenderTypes.h"

namespace Renderer {

/**
 * Provides a platform-agnostic way to create, update, and destroy GPU buffers.
 * Implementations handle the underlying API-specific details (Vulkan, DirectX, etc.)
 */
class GraphicsBuffer {
public:
	virtual ~GraphicsBuffer() = default;

	/**
	 * @brief Create a new GPU buffer
	 * @param desc Buffer description (size, usage, memory type)
	 * @return Handle to the created buffer, or invalid handle on failure
	 */
	virtual BufferHandle createBuffer(const BufferDesc& desc) = 0;

	/**
	 * @brief Destroy a GPU buffer and free its resources
	 * @param handle Handle to the buffer to destroy
	 */
	virtual void destroyBuffer(BufferHandle handle) = 0;

	/**
	 * @brief Update buffer data (for CPU-writable buffers)
	 * @param handle Buffer to update
	 * @param data Pointer to source data
	 * @param size Size of data in bytes
	 * @param offset Offset into buffer (default: 0)
	 */
	virtual void updateBuffer(BufferHandle handle, const void* data, size_t size, size_t offset = 0) = 0;

	/**
	 * @brief Map buffer memory for CPU access
	 * @param handle Buffer to map
	 * @return Pointer to mapped memory, or nullptr on failure
	 */
	virtual void* mapBuffer(BufferHandle handle) = 0;

	/**
	 * @brief Unmap previously mapped buffer memory
	 * @param handle Buffer to unmap
	 */
	virtual void unmapBuffer(BufferHandle handle) = 0;

	/**
	 * @brief Get the size of a buffer in bytes
	 * @param handle Buffer handle
	 * @return Size in bytes, or 0 if invalid
	 */
	virtual size_t getBufferSize(BufferHandle handle) const = 0;
};

} // namespace Renderer
