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
  for (int i = 1; i <= entities; ++i) {
    Entity* entity = &scene->getEntity(i);

    renderEntity(entity, commandBuffer, pipeline, pipelineLayout, imageIndex,
                 0);
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
    return;
  }

  meshComp->render(commandBuffer, pipeline, pipelineLayout, imageIndex,
                   useMousePick);
}

void SceneRenderer::renderOutlineSelected(
    VkCommandBuffer commandBuffer,
    VkPipeline outlinePipeline,
    VkPipelineLayout outlinePipelineLayout,
    VkDescriptorSet outlineDescriptorSet) {
  Scene* scene = SceneManager::getActiveScene();
  size_t entities = scene->getEntityCount();
  for (int i = 1; i <= entities; ++i) {
    Entity* entity = &scene->getEntity(i);
    if (entity->isSelected) {
      int selectedID = entity->getID();
      vkCmdPushConstants(commandBuffer, outlinePipelineLayout,
                         VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(int),
                         &selectedID);

      vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                        outlinePipeline);
      vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              outlinePipelineLayout, 0, 1,
                              &outlineDescriptorSet, 0, nullptr);
      vkCmdDraw(commandBuffer, 3, 1, 0, 0);  // Full-screen triangle
    }
  }
}

void SceneRenderer::renderMousePick(VkCommandBuffer commandBuffer,
                                    VkPipeline pipeline,
                                    VkPipelineLayout pipelineLayout,
                                    uint32_t imageIndex) {
  Scene* scene = SceneManager::getActiveScene();

  size_t entities = scene->getEntityCount();
  for (int i = 1; i <= entities; ++i) {
    Entity* entity = &scene->getEntity(i);

    renderEntity(entity, commandBuffer, pipeline, pipelineLayout, imageIndex,
                 1);
  }
}
