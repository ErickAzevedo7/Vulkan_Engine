#include "SceneRenderer.h"

VulkanCore* SceneRenderer::engineCore = nullptr;

void SceneRenderer::init(VulkanCore* engineCore) {
  SceneRenderer::engineCore = engineCore;
}

void SceneRenderer::renderScene(VkCommandBuffer commandBuffer,
                                VkPipeline pipeline,
                                VkPipelineLayout pipelineLayout,
                                uint32_t imageIndex) {
  Scene* scene = SceneManager::getActiveScene();

  size_t entities = scene->getEntityCount();
  for (int i = 0; i < entities; ++i) {
    Entity* entity = &scene->getEntity(i);

    renderEntity(entity, commandBuffer, pipeline, pipelineLayout, imageIndex, 0);
  }
}

void SceneRenderer::renderEntity(const Entity* entity,
                                 VkCommandBuffer commandBuffer,
                                 VkPipeline pipeline,
                                 VkPipelineLayout pipelineLayout,
                                 uint32_t imageIndex,
																	int useMousePick) {
  const MeshComponent* meshComp = entity->getComponent<MeshComponent>();

  if (!meshComp) {
    throw std::runtime_error("Entity does not have a MeshComponent.");
  }

  const Transform* transformComp = entity->getComponent<Transform>();
  if (transformComp) {
    glm::mat4 modelMatrix = transformComp->getMatrix();
  }

  meshComp->render(commandBuffer, pipeline, pipelineLayout, imageIndex, useMousePick);
}

void SceneRenderer::renderMousePick(VkCommandBuffer commandBuffer,
                                    VkPipeline pipeline,
                                    VkPipelineLayout pipelineLayout,
                                    uint32_t imageIndex) {
  Scene* scene = SceneManager::getActiveScene();

  size_t entities = scene->getEntityCount();
  for (int i = 0; i < entities; ++i) {
    Entity* entity = &scene->getEntity(i);

    renderEntity(entity, commandBuffer, pipeline, pipelineLayout, imageIndex, 1);
  }
}
