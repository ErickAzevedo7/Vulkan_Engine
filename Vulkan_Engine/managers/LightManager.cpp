#include "LightManager.h"

// Simple single-light manager: creates a uniform buffer per-frame and exposes
// it so descriptor sets can bind it at binding 2.

static std::vector<VkBuffer> lightBuffers; 
static std::vector<VkDeviceMemory> lightBufferMem;

void LightManager::init() {
  uint32_t count = MAX_FRAMES_IN_FLIGHT;
  lightBuffers.resize(count);
  lightBufferMem.resize(count);

  // allocate three vec4s: [0]=color+intensity, [1]=direction, [2]=position+type (THIS IS TEMPORARY)
  VkDeviceSize bufferSize = sizeof(glm::vec4) * 3;
  for (uint32_t i = 0; i < count; ++i) {
    Utils::createBuffer(bufferSize,
                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                        lightBuffers[i], lightBufferMem[i]);
  }
}

void LightManager::cleanup() {
  for (size_t i = 0; i < lightBuffers.size(); ++i) {
    vkDestroyBuffer(VulkanCore::getDevice(), lightBuffers[i], nullptr);
    vkFreeMemory(VulkanCore::getDevice(), lightBufferMem[i], nullptr);
  }
  lightBuffers.clear();
  lightBufferMem.clear();
}

void LightManager::updateLight(uint32_t frame, const LightComponent::LightUniform& u) {
  if (frame >= lightBuffers.size()) return;
  void* data;
  // write three vec4s: [0]=rgb=color, a=intensity ; [1]=xyz=direction, w=pad ; [2]=xyz=position, w=type (THIS IS TEMPORARY)
  glm::vec4 v0 = glm::vec4(u.color, u.intensity);
  glm::vec3 dir = glm::normalize(u.direction);
  glm::vec4 v1 = glm::vec4(dir, 0.0f);
  glm::vec4 v2 = glm::vec4(u.position, static_cast<float>(u.type));

  vkMapMemory(VulkanCore::getDevice(), lightBufferMem[frame], 0, sizeof(glm::vec4) * 3, 0, &data);
  memcpy(data, &v0, sizeof(glm::vec4));
  memcpy(reinterpret_cast<char*>(data) + sizeof(glm::vec4), &v1, sizeof(glm::vec4));
  memcpy(reinterpret_cast<char*>(data) + sizeof(glm::vec4) * 2, &v2, sizeof(glm::vec4));
  vkUnmapMemory(VulkanCore::getDevice(), lightBufferMem[frame]);
}

VkBuffer LightManager::getLightBuffer(uint32_t frame) {
  return lightBuffers.at(frame);
}
