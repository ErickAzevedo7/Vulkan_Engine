#pragma once

#include <vulkan/vulkan.h>

#include "renderer/RenderCommandList.h"

namespace Renderer {

/**
 * @brief Vulkan implementation of RenderCommandList.
 * Wraps a VkCommandBuffer to record commands.
 */
class VulkanCommandList : public RenderCommandList {
public:
	/**
	 * @brief Construct wrapping an existing active command buffer.
	 * The command buffer must already be in the recording state.
	 * @param cmd The command buffer
	 */
	explicit VulkanCommandList(VkCommandBuffer cmd);
	~VulkanCommandList() override = default;

	void bindPipeline(void* pipeline) override;
	void bindVertexBuffers(void** buffers, const size_t* offsets, uint32_t count) override;
	void bindIndexBuffer(void* buffer) override;
	void bindDescriptorSets(void* pipelineLayout,
							void** descriptorSets,
							uint32_t setCount,
							uint32_t* dynamicOffsets,
							uint32_t dynamicOffsetCount,
							uint32_t firstSet = 0) override;
	void pushConstants(
		void* pipelineLayout, uint32_t stageFlags, uint32_t offset, uint32_t size, const void* pValues) override;
	void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
	void drawIndexed(uint32_t indexCount,
					 uint32_t instanceCount,
					 uint32_t firstIndex,
					 int32_t vertexOffset,
					 uint32_t firstInstance) override;

private:
	VkCommandBuffer commandBuffer;

public:
	VkCommandBuffer getCommandBuffer() const {
		return commandBuffer;
	}
};

} // namespace Renderer
