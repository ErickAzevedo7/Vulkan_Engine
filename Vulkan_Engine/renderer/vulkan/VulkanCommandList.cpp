#include "VulkanCommandList.h"

#include <cstddef>
#include <cstdint>

#include "vulkan/vulkan_core.h"

namespace Renderer {

VulkanCommandList::VulkanCommandList(VkCommandBuffer cmd) : commandBuffer(cmd) {
}

void VulkanCommandList::bindPipeline(void* pipeline) {
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, static_cast<VkPipeline>(pipeline));
}

void VulkanCommandList::bindVertexBuffers(void** buffers, const size_t* offsets, uint32_t count) {
	// Need to cast the void** array to VkBuffer* array.
	// For small arrays (usually 1), we can just cast or use a local array.
	VkBuffer* vkBuffers = reinterpret_cast<VkBuffer*>(buffers);
	vkCmdBindVertexBuffers(commandBuffer, 0, count, vkBuffers, offsets);
}

void VulkanCommandList::bindIndexBuffer(void* buffer) {
	vkCmdBindIndexBuffer(commandBuffer, static_cast<VkBuffer>(buffer), 0, VK_INDEX_TYPE_UINT32);
}

void VulkanCommandList::bindDescriptorSets(void* pipelineLayout,
										   void** descriptorSets,
										   uint32_t setCount,
										   uint32_t* dynamicOffsets,
										   uint32_t dynamicOffsetCount,
										   uint32_t firstSet) {
	VkDescriptorSet* vkSets = reinterpret_cast<VkDescriptorSet*>(descriptorSets);
	vkCmdBindDescriptorSets(commandBuffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							static_cast<VkPipelineLayout>(pipelineLayout),
							firstSet,
							setCount,
							vkSets,
							dynamicOffsetCount,
							dynamicOffsets);
}

void VulkanCommandList::pushConstants(
	void* pipelineLayout, uint32_t stageFlags, uint32_t offset, uint32_t size, const void* pValues) {
	// Directly casting stageFlags to VkShaderStageFlags (they map 1:1 if we pass Vulkan flags for now)
	vkCmdPushConstants(commandBuffer,
					   static_cast<VkPipelineLayout>(pipelineLayout),
					   static_cast<VkShaderStageFlags>(stageFlags),
					   offset,
					   size,
					   pValues);
}

void VulkanCommandList::draw(uint32_t vertexCount,
							 uint32_t instanceCount,
							 uint32_t firstVertex,
							 uint32_t firstInstance) {
	vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VulkanCommandList::drawIndexed(
	uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance) {
	vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
}

} // namespace Renderer
