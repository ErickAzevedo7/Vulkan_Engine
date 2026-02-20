#include "SceneRenderer.h"

#include <cstddef>
#include <cstdint>

#include "components/MeshComponent.h"
#include "context/ResourceContext.h"
#include "Entity.h"
#include "managers/SceneManager.h"
#include "Scene.h"
#include "vulkan/vulkan_core.h"

// Initialize static members
VulkanCore* SceneRenderer::engineCore = nullptr;
ResourceContext* SceneRenderer::resources = nullptr;

void SceneRenderer::init(VulkanCore* engineCore, ResourceContext* resources) {
	SceneRenderer::engineCore = engineCore;
	SceneRenderer::resources = resources;
}

void SceneRenderer::renderScene(VkCommandBuffer commandBuffer,
								VkPipeline pipeline,
								VkPipelineLayout pipelineLayout,
								uint32_t currentFrame,
								uint64_t dynamicAlignment) {
	Scene* scene = resources->getSceneManager().getActiveScene();

	size_t entities = scene->getEntityCount();
	for (int i = 1; i <= entities; ++i) {
		Entity* entity = &scene->getEntity(i);

		renderEntity(entity, commandBuffer, pipeline, pipelineLayout, currentFrame, dynamicAlignment, 0);
	}
}

void SceneRenderer::renderEntity(const Entity* entity,
								 VkCommandBuffer commandBuffer,
								 VkPipeline pipeline,
								 VkPipelineLayout pipelineLayout,
								 uint32_t currentFrame,
								 uint64_t dynamicAlignment,
								 int useMousePick) {
	const MeshComponent* meshComp = entity->getComponent<MeshComponent>();

	if (!meshComp) {
		return;
	}

	meshComp->render(commandBuffer,
					 pipeline,
					 pipelineLayout,
					 currentFrame,
					 dynamicAlignment,
					 useMousePick,
					 resources->getMeshManager(),
					 resources->getResourceBinder());
}

void SceneRenderer::renderOutlineSelected(VkCommandBuffer commandBuffer,
										  VkPipeline outlinePipeline,
										  VkPipelineLayout outlinePipelineLayout,
										  VkDescriptorSet outlineDescriptorSet) {
	Scene* scene = resources->getSceneManager().getActiveScene();
	size_t entities = scene->getEntityCount();
	for (int i = 1; i <= entities; ++i) {
		Entity* entity = &scene->getEntity(i);
		if (entity->isSelected) {
			int selectedID = entity->getID();
			vkCmdPushConstants(
				commandBuffer, outlinePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int), &selectedID);

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, outlinePipeline);
			vkCmdBindDescriptorSets(commandBuffer,
									VK_PIPELINE_BIND_POINT_GRAPHICS,
									outlinePipelineLayout,
									0,
									1,
									&outlineDescriptorSet,
									0,
									nullptr);
			vkCmdDraw(commandBuffer, 3, 1, 0, 0); // Full-screen triangle
		}
	}
}

void SceneRenderer::renderMousePick(VkCommandBuffer commandBuffer,
									VkPipeline pipeline,
									VkPipelineLayout pipelineLayout,
									uint32_t currentFrame,
									uint64_t dynamicAlignment) {
	Scene* scene = resources->getSceneManager().getActiveScene();

	size_t entities = scene->getEntityCount();
	for (int i = 1; i <= entities; ++i) {
		Entity* entity = &scene->getEntity(i);

		renderEntity(entity, commandBuffer, pipeline, pipelineLayout, currentFrame, dynamicAlignment, 1);
	}
}
