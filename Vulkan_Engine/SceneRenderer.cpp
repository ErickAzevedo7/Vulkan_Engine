#include "SceneRenderer.h"

#include <cstddef>
#include <cstdint>

#include "components/MeshComponent.h"
#include "context/ResourceContext.h"
#include "Entity.h"
#include "managers/SceneManager.h"
#include "renderer/RenderCommandList.h"
#include "Scene.h"


// Initialize static members
ResourceContext* SceneRenderer::resources = nullptr;

void SceneRenderer::init(ResourceContext* resources) {
	SceneRenderer::resources = resources;
}

void SceneRenderer::renderScene(Renderer::RenderCommandList& commandList,
								VkPipeline pipeline,
								VkPipelineLayout pipelineLayout,
								uint32_t currentFrame,
								uint64_t dynamicAlignment) {
	Scene* scene = resources->getSceneManager().getActiveScene();

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
									uint64_t dynamicAlignment) {
	Scene* scene = resources->getSceneManager().getActiveScene();

	size_t entities = scene->getEntityCount();
	for (int i = 1; i <= entities; ++i) {
		Entity* entity = &scene->getEntity(i);

		renderEntity(entity, commandList, pipeline, pipelineLayout, currentFrame, dynamicAlignment, 1);
	}
}
