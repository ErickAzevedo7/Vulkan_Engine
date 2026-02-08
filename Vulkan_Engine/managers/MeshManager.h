#pragma once

#include "vulkan/vulkan_core.h"

#include <glm/gtx/hash.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"

// Forward declarations
class VulkanCore;

// Project headers
#include "core/vulkancore.h"

struct Vertex {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec2 texCoord;

	static VkVertexInputBindingDescription getBindingDescription();

	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();

	bool operator==(const Vertex& other) const {
		return pos == other.pos && normal == other.normal && texCoord == other.texCoord;
	}
};

namespace std {
template<> struct hash<Vertex> {
	inline size_t operator()(Vertex const& vertex) const noexcept {
		return ((std::hash<glm::vec3>()(vertex.pos) ^ (std::hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
			   (std::hash<glm::vec2>()(vertex.texCoord) << 1);
	}
};
} // namespace std

struct Mesh {
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexMemory;
	uint32_t indexCount;
	std::string name;
};

const std::vector<Vertex> quadVertices = {{{-0.5f, -0.5f, 0.0f}, {0, 0, 1}, {0, 0}},
										  {{0.5f, -0.5f, 0.0f}, {0, 0, 1}, {1, 0}},
										  {{0.5f, 0.5f, 0.0f}, {0, 0, 1}, {1, 1}},
										  {{-0.5f, 0.5f, 0.0f}, {0, 0, 1}, {0, 1}}};

const std::vector<Vertex> cubeVertices = {
	// Front face
	{{-0.5f, -0.5f, 0.5f}, {0, 0, 1}, {0, 0}},
	{{0.5f, -0.5f, 0.5f}, {0, 0, 1}, {1, 0}},
	{{0.5f, 0.5f, 0.5f}, {0, 0, 1}, {1, 1}},
	{{-0.5f, 0.5f, 0.5f}, {0, 0, 1}, {0, 1}},
	// Back face
	{{-0.5f, -0.5f, -0.5f}, {0, 0, -1}, {1, 0}},
	{{-0.5f, 0.5f, -0.5f}, {0, 0, -1}, {1, 1}},
	{{0.5f, 0.5f, -0.5f}, {0, 0, -1}, {0, 1}},
	{{0.5f, -0.5f, -0.5f}, {0, 0, -1}, {0, 0}},
	// Left face
	{{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 0}},
	{{-0.5f, -0.5f, 0.5f}, {-1, 0, 0}, {1, 0}},
	{{-0.5f, 0.5f, 0.5f}, {-1, 0, 0}, {1, 1}},
	{{-0.5f, 0.5f, -0.5f}, {-1, 0, 0}, {0, 1}},
	// Right face
	{{0.5f, -0.5f, -0.5f}, {1, 0, 0}, {1, 0}},
	{{0.5f, 0.5f, -0.5f}, {1, 0, 0}, {1, 1}},
	{{0.5f, 0.5f, 0.5f}, {1, 0, 0}, {0, 1}},
	{{0.5f, -0.5f, 0.5f}, {1, 0, 0}, {0, 0}},
	// Top face
	{{-0.5f, 0.5f, -0.5f}, {0, 1, 0}, {0, 1}},
	{{-0.5f, 0.5f, 0.5f}, {0, 1, 0}, {0, 0}},
	{{0.5f, 0.5f, 0.5f}, {0, 1, 0}, {1, 0}},
	{{0.5f, 0.5f, -0.5f}, {0, 1, 0}, {1, 1}},
	// Bottom face
	{{-0.5f, -0.5f, -0.5f}, {0, -1, 0}, {1, 1}},
	{{0.5f, -0.5f, -0.5f}, {0, -1, 0}, {0, 1}},
	{{0.5f, -0.5f, 0.5f}, {0, -1, 0}, {0, 0}},
	{{-0.5f, -0.5f, 0.5f}, {0, -1, 0}, {1, 0}}};

const std::vector<uint32_t> cubeIndices = {
	0,	1,	2,	2,	3,	0, // front
	4,	5,	6,	6,	7,	4, // back
	8,	9,	10, 10, 11, 8, // left
	12, 13, 14, 14, 15, 12, // right
	16, 17, 18, 18, 19, 16, // top
	20, 21, 22, 22, 23, 20 // bottom
};

const std::vector<uint32_t> quadIndices = {0, 1, 2, 2, 3, 0};

class MeshManager {
public:
	static Mesh quad;
	static Mesh cube;
	static Mesh sphere;

	static void loadDefaults(VkCommandPool commandPool, VkQueue graphicsQueue);

	static Mesh createMesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices, VkCommandPool commandPool);

	static void generateSphere(std::vector<Vertex>& vertices,
							   std::vector<uint32_t>& indices,
							   uint32_t X_SEGMENTS,
							   uint32_t Y_SEGMENTS);

	static Mesh* getMesh(std::string name);

	static void cleanup();

private:
	static void createVertexBuffer(std::vector<Vertex> vertices,
								   VkCommandPool commandPool,
								   VkBuffer& vertexBuffer,
								   VkDeviceMemory& vertexBufferMemory);

	static void createIndexBuffer(std::vector<uint32_t> indices,
								  VkCommandPool commandPool,
								  VkBuffer& indexBuffer,
								  VkDeviceMemory& indexBufferMemory);
};
