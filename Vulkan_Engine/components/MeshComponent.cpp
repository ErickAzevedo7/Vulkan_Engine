#include "MeshComponent.h"

#include <cstdint>
#include <stdexcept>
#include <string>

#include "Entity.h"
#include "managers/MaterialManager.h"
#include "managers/MeshManager.h"
#include "renderer/GraphicsResourceBinder.h"
#include "renderer/RenderCommandList.h"
#include "renderer/RenderTypes.h"

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

void MeshComponent::render(Renderer::RenderCommandList& commandList,
						   VkPipeline pipeline,
						   VkPipelineLayout pipelineLayout,
						   uint32_t currentFrame,
						   uint64_t dynamicAlignment,
						   int useMousePick,
						   MeshManager& meshManager,
						   Renderer::GraphicsResourceBinder& binder) const {
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

	commandList.pushConstants(
		pipelineLayout, 16 /* VK_SHADER_STAGE_FRAGMENT_BIT */, 0, sizeof(pushConstants), &pushConstants);

	commandList.bindPipeline(pipeline);

	void* vertexBuffers[] = {meshManager.getVertexBuffer(*mesh)};
	size_t offsets[] = {0};
	commandList.bindVertexBuffers(vertexBuffers, offsets, 1);

	commandList.bindIndexBuffer(meshManager.getIndexBuffer(*mesh));

	uint32_t maxEntities = 1000;
	if (id >= maxEntities) {
		throw std::runtime_error("Entity ID exceeds uniform buffer capacity!");
	}
	uint32_t dynamicOffset = static_cast<uint32_t>(id * dynamicAlignment);

	// Retrieve native handle from binder
	Renderer::ResourceSetHandle frameSet = material->resourceSets[currentFrame];
	VkDescriptorSet vkSet = *static_cast<VkDescriptorSet*>(binder.getNativeHandle(frameSet));

	// Bind descriptor sets (for uniforms/textures)
	void* vkSetPtr = vkSet;
	commandList.bindDescriptorSets(pipelineLayout, &vkSetPtr, 1, &dynamicOffset, 1);

	// Issue draw call
	commandList.drawIndexed(mesh->indexCount, 1, 0, 0, 0);
}

Mesh* MeshComponent::GetMesh() const {
	return mesh;
}

Entity* MeshComponent::GetOwner() const {
	return owner;
}
