#pragma once
#include <vector>
#include "core/vulkancore.h"
#include "components/Component.h"
#include <vulkan/vulkan.h>
class Entity
{
public:
	Entity(std::string name);
	Entity(std::vector<Vertex> vertices, std::vector<uint32_t> indices, VulkanCore* engineCore);
	~Entity();
	std::string getName();
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	VkBuffer vertexBuffer;
	VkBuffer indexBuffer;
private:
	std::vector<Component> components;
	std::string name;
	//static VulkanCore* engineCore;
};

