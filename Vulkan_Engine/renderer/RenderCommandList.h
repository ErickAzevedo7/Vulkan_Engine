#pragma once

#include <cstddef>
#include <cstdint>

namespace Renderer {

/**
 * @brief Abstract interface for recording rendering commands.
 * This decoupled interface hides the underlying graphics API (Vulkan, DirectX, etc.)
 * from the high-level scene and gameplay code.
 */
class RenderCommandList {
public:
	virtual ~RenderCommandList() = default;

	virtual void bindPipeline(void* pipeline) = 0;

	virtual void bindVertexBuffers(void** buffers, const size_t* offsets, uint32_t count) = 0;

	virtual void bindIndexBuffer(void* buffer) = 0;

	/**
	 * @brief Bind descriptor sets (uniforms, textures)
	 * @param pipelineLayout Native pipeline layout handle
	 * @param descriptorSets Array of native descriptor set handles
	 * @param setCount Number of sets
	 * @param dynamicOffsets Optional array of dynamic offsets
	 * @param dynamicOffsetCount Number of dynamic offsets
	 */
	virtual void bindDescriptorSets(void* pipelineLayout,
									void** descriptorSets,
									uint32_t setCount,
									uint32_t* dynamicOffsets,
									uint32_t dynamicOffsetCount,
									uint32_t firstSet = 0) = 0;

	/**
	 * @brief Push constant data directly to the shader
	 * @param pipelineLayout Native pipeline layout handle
	 * @param stageFlags Abstracted to uint32_t for now (API-specific flags mapped internally or raw)
	 * @param offset Byte offset
	 * @param size Data size in bytes
	 * @param pValues Pointer to the data
	 */
	virtual void
	pushConstants(void* pipelineLayout, uint32_t stageFlags, uint32_t offset, uint32_t size, const void* pValues) = 0;

	virtual void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) = 0;

	virtual void drawIndexed(uint32_t indexCount,
							 uint32_t instanceCount,
							 uint32_t firstIndex,
							 int32_t vertexOffset,
							 uint32_t firstInstance) = 0;
};

} // namespace Renderer
