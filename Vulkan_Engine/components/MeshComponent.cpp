#include "MeshComponent.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

#include "Entity.h"
#include "managers/MaterialManager.h"
#include "managers/MeshManager.h"
#include "renderer/GraphicsResourceBinder.h"
#include "renderer/RenderCommandList.h"
#include "renderer/RenderTypes.h"
#include "SceneRenderer.h"
#include "vulkan/vulkan_core.h"

#include "glm/ext/vector_float3.hpp"

MeshComponent::MeshComponent(Entity* owner, const std::string& meshName, MeshManager& meshManager)
	: Component(), visible(true) {
	this->owner = owner;
	mesh = meshManager.getMesh(meshName);

	this->material = nullptr;

	if (!mesh) {
		std::cerr << "[MeshComponent] Warning: Mesh not found: " << meshName << std::endl;
	}
}

MeshComponent::~MeshComponent() {
	mesh = nullptr;
}

void MeshComponent::render(Renderer::RenderCommandList& commandList,
						   VkPipeline pipeline,
						   VkPipelineLayout pipelineLayout,
						   uint32_t currentFrame,
						   int useMousePick,
						   uint32_t perObjectIndex,
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

	commandList.pushConstants(pipelineLayout,
							  VK_SHADER_STAGE_VERTEX_BIT |
								  VK_SHADER_STAGE_FRAGMENT_BIT, // must cover all stages in the pipeline layout range
							  0,
							  sizeof(pushConstants),
							  &pushConstants);

	commandList.bindPipeline(pipeline);

	void* vertexBuffers[] = {meshManager.getVertexBuffer(*mesh)};
	size_t offsets[] = {0};
	commandList.bindVertexBuffers(vertexBuffers, offsets, 1);

	commandList.bindIndexBuffer(meshManager.getIndexBuffer(*mesh));

	const uint32_t maxEntities = SceneRenderer::getMaxObjects();
	if (perObjectIndex >= maxEntities) {
		static bool capacityWarningLogged = false;
		if (!capacityWarningLogged) {
			std::cerr << "[MeshComponent] Per-object uniform buffer capacity exceeded; skipping extra draws."
					  << std::endl;
			capacityWarningLogged = true;
		}
		return;
	}
	uint32_t dynamicOffset = static_cast<uint32_t>(perObjectIndex * SceneRenderer::getDynamicAlignment());

	// Retrieve native handle for Material (Set 2) from binder
	Renderer::ResourceSetHandle frameSet = material->resourceSets[currentFrame];
	VkDescriptorSet matSet = *static_cast<VkDescriptorSet*>(binder.getNativeHandle(frameSet));

	// Retrieve native handle for PerObject (Set 3) from SceneRenderer
	VkDescriptorSet perObjSet = SceneRenderer::getPerObjectDescriptorSets()[currentFrame];

	// Bind material descriptor set at slot 2, per-object set at slot 3
	std::array<VkDescriptorSet, 2> setsToBind = {matSet, perObjSet};

	commandList.bindDescriptorSets(pipelineLayout,
								   reinterpret_cast<void**>(setsToBind.data()),
								   2 /*descriptorSetCount*/,
								   &dynamicOffset,
								   1 /*dynamicOffsetCount*/,
								   2 /*firstSet=2*/);

	// Issue draw call
	commandList.drawIndexed(mesh->indexCount, 1, 0, 0, 0);
}

Mesh* MeshComponent::GetMesh() const {
	return mesh;
}

Entity* MeshComponent::GetOwner() const {
	return owner;
}
