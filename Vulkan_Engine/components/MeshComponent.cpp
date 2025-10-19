#include "MeshComponent.h"

MeshComponent::MeshComponent(Entity* owner,
                             MeshManager* meshManager,
                             const std::string& meshName)
    : Component(), owner(owner), meshManager(meshManager), visible(true) {
  if (!meshManager) {
    throw std::invalid_argument("MeshManager pointer is null.");
  }
  mesh = MeshManager::getMesh(
      meshName);
  if (!mesh) {
    throw std::runtime_error("Mesh not found: " + meshName);
  }
}

MeshComponent::~MeshComponent() {
  
}

void MeshComponent::render(VkCommandBuffer commandBuffer,
                           VkPipeline pipeline,
                           VkPipelineLayout pipelineLayout,
                           VkDescriptorSet descriptorSet) {
  if (!visible || !mesh)
    return;

  // Bind pipeline
  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

  // Bind vertex buffer
  VkBuffer vertexBuffers[] = {mesh->vertexBuffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

  // Bind index buffer
  vkCmdBindIndexBuffer(commandBuffer, mesh->indexBuffer, 0,
                       VK_INDEX_TYPE_UINT32);

  // Bind descriptor sets (for uniforms/textures)
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

  // Issue draw call
  vkCmdDrawIndexed(commandBuffer, mesh->indexCount, 1, 0, 0, 0);
}

Mesh* MeshComponent::GetMesh() const {
  return mesh;
}

Entity* MeshComponent::GetOwner() const {
  return owner;
}
