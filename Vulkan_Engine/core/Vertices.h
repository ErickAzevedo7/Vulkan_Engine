#pragma once
#include <vulkan/vulkan.h>
#include "vulkancore.h"
#include "core/utils/Utils.h"

class Vertices
{
public:
    static void createVertexBuffer(VkCommandPool commandPool, VkBuffer& vertexBuffer, VkDeviceMemory& vertexBufferMemory);

    static void createIndexBuffer(VkCommandPool commandPool, VkBuffer& indexBuffer, VkDeviceMemory& indexBufferMemory);
};

