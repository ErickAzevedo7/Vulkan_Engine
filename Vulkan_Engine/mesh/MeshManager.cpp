#include "MeshManager.h"
#define _USE_MATH_DEFINES
#include "math.h"

Mesh MeshManager::cube;
Mesh MeshManager::quad;
Mesh MeshManager::sphere;

void MeshManager::generateSphere(std::vector<Vertex>& vertices,
                                 std::vector<uint32_t>& indices,
                                 uint32_t X_SEGMENTS = 32,
                                 uint32_t Y_SEGMENTS = 16) {
  vertices.clear();
  indices.clear();

  for (uint32_t y = 0; y <= Y_SEGMENTS; y++) {
    for (uint32_t x = 0; x <= X_SEGMENTS; x++) {
      float xSegment = (float)x / (float)X_SEGMENTS;
      float ySegment = (float)y / (float)Y_SEGMENTS;
      float xPos = std::cos(xSegment * 2.0f * M_PI) * std::sin(ySegment * M_PI);
      float yPos = std::cos(ySegment * M_PI);
      float zPos = std::sin(xSegment * 2.0f * M_PI) * std::sin(ySegment * M_PI);

      vertices.push_back({
          {xPos * 0.5f, yPos * 0.5f, zPos * 0.5f},  // position
          {xPos, yPos, zPos},                       // normal
          {xSegment, ySegment}                      // uv
      });
    }
  }

  bool oddRow = false;
  for (uint32_t y = 0; y < Y_SEGMENTS; ++y) {
    for (uint32_t x = 0; x <= X_SEGMENTS; ++x) {
      indices.push_back(y * (X_SEGMENTS + 1) + x);
      indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
    }
  }
}

VkVertexInputBindingDescription Vertex::getBindingDescription() {
  VkVertexInputBindingDescription bindingDescription{};
  bindingDescription.binding = 0;
  bindingDescription.stride = sizeof(Vertex);
  bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 3>
Vertex::getAttributeDescriptions() {
  std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

  attributeDescriptions[0].binding = 0;
  attributeDescriptions[0].location = 0;
  attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[0].offset = offsetof(Vertex, pos);

  attributeDescriptions[1].binding = 0;
  attributeDescriptions[1].location = 1;
  attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  attributeDescriptions[1].offset = offsetof(Vertex, color);

  attributeDescriptions[2].binding = 0;
  attributeDescriptions[2].location = 2;
  attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
  attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

  return attributeDescriptions;
}

Mesh* MeshManager::getMesh(std::string name) {
  if (name == "quad") {
    return &quad;
  } else if (name == "cube") {
    return &cube;
  } else if (name == "sphere") {
    return &sphere;
  } else {
    throw std::runtime_error("Mesh not found: " + name);
  }
}

void MeshManager::loadDefaults(VkCommandPool commandPool,
                               VkQueue graphicsQueue) {
  quad = createMesh(quadVertices, quadIndices, commandPool);
  cube = createMesh(cubeVertices, cubeIndices, commandPool);
  std::vector<Vertex> sphereVerts;
  std::vector<uint32_t> sphereInds;
  generateSphere(sphereVerts, sphereInds);
  sphere = createMesh(sphereVerts, sphereInds, commandPool);
}

Mesh MeshManager::createMesh(std::vector<Vertex> vertices,
                             std::vector<uint32_t> indices,
                             VkCommandPool commandPool) {
  Mesh mesh;

  createVertexBuffer(vertices, commandPool, mesh.vertexBuffer,
                     mesh.vertexMemory);

  createIndexBuffer(indices, commandPool, mesh.indexBuffer, mesh.indexMemory);

  mesh.indexCount = static_cast<uint32_t>(indices.size());

  return mesh;
}

void MeshManager::cleanup(VkDevice device) {
  vkDestroyBuffer(device, quad.vertexBuffer, nullptr);
  vkFreeMemory(device, quad.vertexMemory, nullptr);
  vkDestroyBuffer(device, quad.indexBuffer, nullptr);
  vkFreeMemory(device, quad.indexMemory, nullptr);
  vkDestroyBuffer(device, cube.vertexBuffer, nullptr);
  vkFreeMemory(device, cube.vertexMemory, nullptr);
  vkDestroyBuffer(device, cube.indexBuffer, nullptr);
  vkFreeMemory(device, cube.indexMemory, nullptr);
  vkDestroyBuffer(device, sphere.vertexBuffer, nullptr);
  vkFreeMemory(device, sphere.vertexMemory, nullptr);
  vkDestroyBuffer(device, sphere.indexBuffer, nullptr);
  vkFreeMemory(device, sphere.indexMemory, nullptr);
}

void MeshManager::createVertexBuffer(std::vector<Vertex> vertices,
                                     VkCommandPool commandPool,
                                     VkBuffer& vertexBuffer,
                                     VkDeviceMemory& vertexBufferMemory) {
  VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  Utils::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(VulkanCore::getDevice(), stagingBufferMemory, 0, bufferSize, 0,
              &data);
  memcpy(data, vertices.data(), (size_t)bufferSize);
  vkUnmapMemory(VulkanCore::getDevice(), stagingBufferMemory);

  Utils::createBuffer(
      bufferSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

  Utils::copyBuffer(commandPool, stagingBuffer, vertexBuffer, bufferSize);

  vkDestroyBuffer(VulkanCore::getDevice(), stagingBuffer, nullptr);
  vkFreeMemory(VulkanCore::getDevice(), stagingBufferMemory, nullptr);
}

void MeshManager::createIndexBuffer(std::vector<uint32_t> indices,
                                    VkCommandPool commandPool,
                                    VkBuffer& indexBuffer,
                                    VkDeviceMemory& indexBufferMemory) {
  VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;
  Utils::createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      stagingBuffer, stagingBufferMemory);

  void* data;
  vkMapMemory(VulkanCore::getDevice(), stagingBufferMemory, 0, bufferSize, 0,
              &data);
  memcpy(data, indices.data(), (size_t)bufferSize);
  vkUnmapMemory(VulkanCore::getDevice(), stagingBufferMemory);

  Utils::createBuffer(
      bufferSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

  Utils::copyBuffer(commandPool, stagingBuffer, indexBuffer, bufferSize);

  vkDestroyBuffer(VulkanCore::getDevice(), stagingBuffer, nullptr);
  vkFreeMemory(VulkanCore::getDevice(), stagingBufferMemory, nullptr);
}
