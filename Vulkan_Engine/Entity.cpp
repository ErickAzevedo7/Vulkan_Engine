#include "Entity.h"

Entity::Entity(std::string name)
{
  this->name = name;
}

Entity::Entity(std::vector<Vertex> vertices, std::vector<uint32_t> indices, VulkanCore* engineCore)
{
	this->vertices = vertices;
	this->indices = indices;

	//vertices = engineCore->createVertexBuffer();
	//indexBuffer = engineCore->createIndexBuffer();
}
Entity::~Entity()
{
	return;
}

std::string Entity::getName() {
	return name;
}
