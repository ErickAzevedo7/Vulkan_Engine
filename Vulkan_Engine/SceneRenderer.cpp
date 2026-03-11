#include "SceneRenderer.h"

#include <cstddef>
#include <cstdint>

#include "components/MeshComponent.h"
#include "components/Transform.h"
#include "context/ResourceContext.h"
#include "Entity.h"
#include "managers/MeshManager.h"
#include "managers/SceneManager.h"
#include "renderer/RenderCommandList.h"
#include "renderer/vulkan/VulkanCommandList.h"
#include "Scene.h"
#include "vulkan/vulkan_core.h"

#include "glm/ext/matrix_float4x4.hpp"

// Initialize static members
ResourceContext* SceneRenderer::resources = nullptr;

void SceneRenderer::init(ResourceContext* resources) {
	SceneRenderer::resources = resources;
}

void SceneRenderer::renderScene(Renderer::RenderCommandList& commandList,
								VkPipeline pipeline,
								VkPipelineLayout pipelineLayout,
								uint32_t currentFrame,
								uint64_t dynamicAlignment,
								VkDescriptorSet globalSet) {
	Scene* scene = resources->getSceneManager().getActiveScene();

	// Bind GlobalUBO (set=0) once at the top of every scene pass.
	// Call vkCmdBindDescriptorSets directly to avoid the void* abstraction
	// round-trip that can corrupt the VkDescriptorSet handle.
	if (auto* vkList = dynamic_cast<Renderer::VulkanCommandList*>(&commandList)) {
		vkCmdBindDescriptorSets(vkList->getCommandBuffer(),
								VK_PIPELINE_BIND_POINT_GRAPHICS,
								pipelineLayout,
								0 /*firstSet*/,
								1,
								&globalSet,
								0,
								nullptr);
	}

	size_t entities = scene->getEntityCount();
	for (int i = 1; i <= entities; ++i) {
		Entity* entity = &scene->getEntity(i);

		renderEntity(entity, commandList, pipeline, pipelineLayout, currentFrame, dynamicAlignment, 0);
	}
}

void SceneRenderer::renderEntity(const Entity* entity,
								 Renderer::RenderCommandList& commandList,
								 VkPipeline pipeline,
								 VkPipelineLayout pipelineLayout,
								 uint32_t currentFrame,
								 uint64_t dynamicAlignment,
								 int useMousePick) {
	const MeshComponent* meshComp = entity->getComponent<MeshComponent>();

	if (!meshComp) {
		return;
	}

	meshComp->render(commandList,
					 pipeline,
					 pipelineLayout,
					 currentFrame,
					 dynamicAlignment,
					 useMousePick,
					 resources->getMeshManager(),
					 resources->getResourceBinder());
}

void SceneRenderer::renderOutlineSelected(Renderer::RenderCommandList& commandList,
										  VkPipeline outlinePipeline,
										  VkPipelineLayout outlinePipelineLayout,
										  VkDescriptorSet outlineDescriptorSet) {
	Scene* scene = resources->getSceneManager().getActiveScene();
	size_t entities = scene->getEntityCount();
	for (int i = 1; i <= entities; ++i) {
		Entity* entity = &scene->getEntity(i);
		if (entity->isSelected) {
			int selectedID = entity->getID();
			commandList.pushConstants(outlinePipelineLayout,
									  16, // VK_SHADER_STAGE_FRAGMENT_BIT mapping
									  0,
									  sizeof(int),
									  &selectedID);

			commandList.bindPipeline(outlinePipeline);
			void* descSets[] = {outlineDescriptorSet};
			commandList.bindDescriptorSets(outlinePipelineLayout, descSets, 1, nullptr, 0);
			commandList.draw(3, 1, 0, 0); // Full-screen triangle
		}
	}
}

void SceneRenderer::renderMousePick(Renderer::RenderCommandList& commandList,
									VkPipeline pipeline,
									VkPipelineLayout pipelineLayout,
									uint32_t currentFrame,
									uint64_t dynamicAlignment,
									VkDescriptorSet globalSet) {
	Scene* scene = resources->getSceneManager().getActiveScene();

	// Bind GlobalUBO (set=0) — required by the vertex shader before any indexed draw.
	if (auto* vkList = dynamic_cast<Renderer::VulkanCommandList*>(&commandList)) {
		vkCmdBindDescriptorSets(vkList->getCommandBuffer(),
								VK_PIPELINE_BIND_POINT_GRAPHICS,
								pipelineLayout,
								0 /*firstSet*/,
								1,
								&globalSet,
								0,
								nullptr);
	}

	size_t entities = scene->getEntityCount();
	for (int i = 1; i <= entities; ++i) {
		Entity* entity = &scene->getEntity(i);

		renderEntity(entity, commandList, pipeline, pipelineLayout, currentFrame, dynamicAlignment, 1);
	}
}

void SceneRenderer::renderShadows(Renderer::RenderCommandList& commandList,
								  VkPipeline shadowPipeline,
								  VkPipelineLayout shadowPipelineLayout,
								  VkDescriptorSet shadowDescriptorSet) {
	Scene* scene = resources->getSceneManager().getActiveScene();
	if (!scene)
		return;

	// Bind shadow pipeline and its descriptor set before drawing anything
	commandList.bindPipeline(shadowPipeline);

	void* pDescriptorSets[] = {shadowDescriptorSet};
	commandList.bindDescriptorSets(shadowPipelineLayout, pDescriptorSets, 1, nullptr, 0);

	auto entities = scene->getEntities();
	MeshManager& meshManager = resources->getMeshManager();

	for (const auto& entPtr : *entities) {
		Entity* e = entPtr.get();
		if (!e)
			continue;
		auto* meshComp = e->getComponent<MeshComponent>();
		auto* transform = e->getComponent<Transform>();
		if (!meshComp || !transform)
			continue;

		Mesh* mesh = meshComp->GetMesh();
		if (!mesh)
			continue;

		// Push the model matrix (64 bytes) for this object
		glm::mat4 model = transform->getMatrix();
		commandList.pushConstants(shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);

		// Bind Geometry Buffers
		void* pBuffers[] = {meshManager.getVertexBuffer(*mesh)};
		size_t offsets[] = {0};
		commandList.bindVertexBuffers(pBuffers, offsets, 1);
		commandList.bindIndexBuffer(meshManager.getIndexBuffer(*mesh));

		commandList.drawIndexed(mesh->indexCount, 1, 0, 0, 0);
	}
}
