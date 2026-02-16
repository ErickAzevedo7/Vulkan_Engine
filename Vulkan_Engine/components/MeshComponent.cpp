#include "MeshComponent.h"

#include <cstdint>
#include <stdexcept>
#include <string>

#include "Entity.h"
#include "managers/MaterialManager.h"
#include "managers/MeshManager.h"
#include "vulkan/vulkan_core.h"

#include "glm/ext/vector_float3.hpp"

MeshComponent::MeshComponent(Entity* owner, const std::string& meshName, MeshManager& meshManager)
	: Component(), owner(owner), visible(true) {
	mesh = meshManager.getMesh(meshName);

	this->material = nullptr;

	if (!mesh) {
		throw std::runtime_error("Mesh not found: " + meshName);
	}
}

MeshComponent::~MeshComponent() {
	mesh = nullptr;
	owner = nullptr;
}

void MeshComponent::render(VkCommandBuffer commandBuffer,
						   VkPipeline pipeline,
						   VkPipelineLayout pipelineLayout,
						   uint32_t imageIndex,
						   int useMousePick,
						   MeshManager& meshManager) const {
	if (!visible || !mesh)
		return;

	uint32_t id = owner->getID();

	struct alignas(16) {
		glm::vec3 pickColor;
		int usePickColor;
	} pushConstants;

	if (useMousePick) {
		uint32_t id = owner->getID();
		pushConstants.pickColor = glm::vec3(
			((id & 0x000000FF) >> 0) / 255.0f, ((id & 0x0000FF00) >> 8) / 255.0f, ((id & 0x00FF0000) >> 16) / 255.0f);
	} else {
		pushConstants.pickColor = glm::vec3(0.0f);
	}

	pushConstants.usePickColor = useMousePick;

	vkCmdPushConstants(
		commandBuffer, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushConstants), &pushConstants);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

	VkBuffer vertexBuffers[] = {meshManager.getVertexBuffer(*mesh)};
	VkDeviceSize offsets[] = {0};
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

	vkCmdBindIndexBuffer(commandBuffer, meshManager.getIndexBuffer(*mesh), 0, VK_INDEX_TYPE_UINT32);

	uint32_t maxEntities = 1000;
	if (id >= maxEntities) {
		throw std::runtime_error("Entity ID exceeds uniform buffer capacity!");
	}
	uint32_t dynamicOffset = static_cast<uint32_t>(id * VulkanCore::getDynamicAlignment());

	// Bind descriptor sets (for uniforms/textures)
	vkCmdBindDescriptorSets(commandBuffer,
							VK_PIPELINE_BIND_POINT_GRAPHICS,
							pipelineLayout,
							0,
							1,
							&material->descriptorSets[VulkanCore::getCurrentFrame()],
							1,
							&dynamicOffset);

	// Issue draw call
	vkCmdDrawIndexed(commandBuffer, mesh->indexCount, 1, 0, 0, 0);
}

Mesh* MeshComponent::GetMesh() const {
	return mesh;
}

Entity* MeshComponent::GetOwner() const {
	return owner;
}
