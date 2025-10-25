#include "MeshComponent.h"

MeshComponent::MeshComponent(Entity* owner,
                             const std::string& meshName)
    : Component(), owner(owner), visible(true) {
  mesh = MeshManager::getMesh(
      meshName);

  this->material = MaterialManager::getMaterial("default");

  if (!mesh) {
    throw std::runtime_error("Mesh not found: " + meshName);
  }
  if (!material) {
    throw std::runtime_error("Material not found: default");
  }
}

MeshComponent::~MeshComponent() {
  mesh = nullptr;
  owner = nullptr;
}

void MeshComponent::render(VkCommandBuffer commandBuffer,
                           VkPipeline pipeline,
                           VkPipelineLayout pipelineLayout,
                           uint32_t imageIndex) const {
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
                          pipelineLayout, 0, 1, &material->descriptorSets[VulkanCore::getCurrentFrame()], 0, nullptr);

  // Issue draw call
  vkCmdDrawIndexed(commandBuffer, mesh->indexCount, 1, 0, 0, 0);
}

Mesh* MeshComponent::GetMesh() const {
  return mesh;
}

Entity* MeshComponent::GetOwner() const {
  return owner;
}
