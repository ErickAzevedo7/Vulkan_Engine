#include "SceneRenderer.h"

VulkanCore* SceneRenderer::engineCore = nullptr;

void SceneRenderer::init(VulkanCore* engineCore) {
  SceneRenderer::engineCore = engineCore;
}

void SceneRenderer::RenderScene(VkCommandBuffer commandBuffer,
                                VkPipeline pipeline,
                                VkPipelineLayout pipelineLayout,
                                VkDescriptorSet descriptorSet) {
  Scene* scene = SceneManager::getActiveScene();

  size_t entities = scene->getEntityCount();
  for (int i = 0; i < entities; ++i) {
    Entity* entity = &scene->getEntity(i);

    RenderEntity(entity, commandBuffer, pipeline, pipelineLayout,
                 descriptorSet);
  }
}

void SceneRenderer::RenderEntity(const Entity* entity,
                                 VkCommandBuffer commandBuffer,
                                 VkPipeline pipeline,
                                 VkPipelineLayout pipelineLayout,
                                 VkDescriptorSet descriptorSet) {
  const MeshComponent* meshComp = entity->getComponent<MeshComponent>();

  if (!meshComp) {
    throw std::runtime_error("Entity does not have a MeshComponent.");
  }

  const Transform* transformComp = entity->getComponent<Transform>();

  meshComp->render(commandBuffer, pipeline, pipelineLayout, descriptorSet);
}
