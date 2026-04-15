#include "MeshManager.h"

#include <array>
#include <cctype>
#include <cmath>
#include <corecrt_math_defines.h>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <limits>

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "core/utils/Utils.h"
#include "renderer/GraphicsBuffer.h"
#include "renderer/RenderTypes.h"
#include "renderer/vulkan/VulkanBuffer.h"
#include "vulkan/vulkan_core.h"

MeshManager::MeshManager() {
	// Constructor
}

MeshManager::~MeshManager() {
	cleanup();
}

void MeshManager::setBufferManager(Renderer::GraphicsBuffer* bufferMgr) {
	bufferManager = bufferMgr;
}

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
			float xPos =
				std::cos(xSegment * 2.0f * static_cast<float>(M_PI)) * std::sin(ySegment * static_cast<float>(M_PI));
			float yPos = std::cos(ySegment * static_cast<float>(M_PI));
			float zPos =
				std::sin(xSegment * 2.0f * static_cast<float>(M_PI)) * std::sin(ySegment * static_cast<float>(M_PI));

			vertices.push_back({
				{xPos * 0.5f, yPos * 0.5f, zPos * 0.5f}, // position
				{xPos, yPos, zPos}, // normal
				{xSegment, ySegment} // uv
			});
		}
	}

	for (uint32_t y = 0; y < Y_SEGMENTS; ++y) {
		for (uint32_t x = 0; x < X_SEGMENTS; ++x) {
			uint32_t first = y * (X_SEGMENTS + 1) + x;
			uint32_t second = first + X_SEGMENTS + 1;

			indices.push_back(first);
			indices.push_back(first + 1);
			indices.push_back(second + 1);

			indices.push_back(second + 1);
			indices.push_back(second);
			indices.push_back(first);
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

std::array<VkVertexInputAttributeDescription, 3> Vertex::getAttributeDescriptions() {
	std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions{};

	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[0].offset = offsetof(Vertex, pos);

	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
	attributeDescriptions[1].offset = offsetof(Vertex, normal);

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
	}

	auto it = importedMeshes.find(name);
	if (it != importedMeshes.end()) {
		return &it->second;
	}

	auto partsIt = importedMeshParts.find(name);
	if (partsIt != importedMeshParts.end() && !partsIt->second.empty()) {
		auto firstPartIt = importedMeshes.find(partsIt->second.front());
		if (firstPartIt != importedMeshes.end()) {
			return &firstPartIt->second;
		}
	}

	return nullptr;
}

bool MeshManager::isSupportedModelFile(const std::string& filePath) {
	std::string ext = std::filesystem::path(filePath).extension().string();
	for (char& c : ext) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}

	return ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb";
}

Mesh* MeshManager::loadMeshFromFile(const std::string& filePath, VkCommandPool commandPool) {
	if (!bufferManager) {
		return nullptr;
	}

	if (!isSupportedModelFile(filePath)) {
		return nullptr;
	}

	const std::string meshKey = std::filesystem::path(filePath).generic_string();
	auto it = importedMeshes.find(meshKey);
	if (it != importedMeshes.end()) {
		return &it->second;
	}

	auto partsIt = importedMeshParts.find(meshKey);
	if (partsIt != importedMeshParts.end() && !partsIt->second.empty()) {
		auto firstPartIt = importedMeshes.find(partsIt->second.front());
		if (firstPartIt != importedMeshes.end()) {
			return &firstPartIt->second;
		}
	}

	std::vector<std::string> partKeys = loadMeshPartsFromFile(filePath, commandPool);
	if (!partKeys.empty()) {
		auto firstPartIt = importedMeshes.find(partKeys.front());
		if (firstPartIt != importedMeshes.end()) {
			return &firstPartIt->second;
		}
	}

	return nullptr;
}

std::vector<std::string> MeshManager::loadMeshPartsFromFile(const std::string& filePath, VkCommandPool commandPool) {
	if (!bufferManager) {
		return {};
	}

	if (!isSupportedModelFile(filePath)) {
		return {};
	}

	const std::string meshKey = std::filesystem::path(filePath).generic_string();
	auto cachedPartsIt = importedMeshParts.find(meshKey);
	if (cachedPartsIt != importedMeshParts.end()) {
		return cachedPartsIt->second;
	}

	std::string ext = std::filesystem::path(filePath).extension().string();
	for (char& c : ext) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}

	unsigned int postProcessFlags = aiProcess_Triangulate | aiProcess_GenSmoothNormals |
		aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality;
	if (ext != ".obj") {
		postProcessFlags |= aiProcess_PreTransformVertices;
	}

	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(filePath, postProcessFlags);
	if (!scene || !scene->HasMeshes()) {
		return {};
	}

	std::vector<std::string> createdPartKeys;
	createdPartKeys.reserve(scene->mNumMeshes);
	std::unordered_map<std::string, uint32_t> nameUsage;

	for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		const aiMesh* srcMesh = scene->mMeshes[meshIndex];
		if (!srcMesh || srcMesh->mNumVertices == 0) {
			continue;
		}

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		vertices.reserve(srcMesh->mNumVertices);
		indices.reserve(srcMesh->mNumFaces * 3);

		for (unsigned int v = 0; v < srcMesh->mNumVertices; ++v) {
			Vertex vertex{};
			const aiVector3D& p = srcMesh->mVertices[v];
			vertex.pos = {p.x, p.y, p.z};

			if (srcMesh->HasNormals()) {
				const aiVector3D& n = srcMesh->mNormals[v];
				vertex.normal = {n.x, n.y, n.z};
			} else {
				vertex.normal = {0.0f, 1.0f, 0.0f};
			}

			if (srcMesh->HasTextureCoords(0)) {
				const aiVector3D& uv = srcMesh->mTextureCoords[0][v];
				vertex.texCoord = {uv.x, uv.y};
			} else {
				vertex.texCoord = {0.0f, 0.0f};
			}

			vertices.push_back(vertex);
		}

		for (unsigned int f = 0; f < srcMesh->mNumFaces; ++f) {
			const aiFace& face = srcMesh->mFaces[f];
			if (face.mNumIndices < 3) {
				continue;
			}

			for (unsigned int i = 0; i < face.mNumIndices; ++i) {
				indices.push_back(face.mIndices[i]);
			}
		}

		if (vertices.empty() || indices.empty()) {
			continue;
		}

		std::string partName;
		if (srcMesh->mName.length > 0) {
			partName = srcMesh->mName.C_Str();
		}
		if (partName.empty()) {
			partName = "Mesh" + std::to_string(meshIndex);
		}

		auto [nameIt, inserted] = nameUsage.emplace(partName, 0);
		if (!inserted) {
			nameIt->second += 1;
			partName += "_" + std::to_string(nameIt->second);
		}

		Mesh mesh = createMesh(std::move(vertices), std::move(indices), commandPool);
		mesh.name = meshKey + "::" + partName;
		auto [insertedIt, wasInserted] = importedMeshes.emplace(mesh.name, std::move(mesh));
		if (wasInserted) {
			createdPartKeys.push_back(insertedIt->first);
		} else {
			createdPartKeys.push_back(mesh.name);
		}
	}

	if (createdPartKeys.empty()) {
		return {};
	}

	importedMeshParts.emplace(meshKey, createdPartKeys);
	return createdPartKeys;
}
void MeshManager::loadDefaults(VkCommandPool commandPool, VkQueue graphicsQueue) {
	if (!bufferManager) {
		throw std::runtime_error("MeshManager: Buffer manager not set before loadDefaults!");
	}
	quad = createMesh(quadVertices, quadIndices, commandPool);
	quad.name = "quad";
	cube = createMesh(cubeVertices, cubeIndices, commandPool);
	cube.name = "cube";
	std::vector<Vertex> sphereVerts;
	std::vector<uint32_t> sphereInds;
	generateSphere(sphereVerts, sphereInds);
	sphere = createMesh(sphereVerts, sphereInds, commandPool);
	sphere.name = "sphere";
}

Mesh MeshManager::createMesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices, VkCommandPool commandPool) {
	Mesh mesh;
	mesh.localBoundsMin = glm::vec3(std::numeric_limits<float>::max());
	mesh.localBoundsMax = glm::vec3(std::numeric_limits<float>::lowest());
	mesh.collisionVertices.reserve(vertices.size());

	for (const Vertex& vertex : vertices) {
		mesh.localBoundsMin.x = std::min(mesh.localBoundsMin.x, vertex.pos.x);
		mesh.localBoundsMin.y = std::min(mesh.localBoundsMin.y, vertex.pos.y);
		mesh.localBoundsMin.z = std::min(mesh.localBoundsMin.z, vertex.pos.z);

		mesh.localBoundsMax.x = std::max(mesh.localBoundsMax.x, vertex.pos.x);
		mesh.localBoundsMax.y = std::max(mesh.localBoundsMax.y, vertex.pos.y);
		mesh.localBoundsMax.z = std::max(mesh.localBoundsMax.z, vertex.pos.z);

		mesh.collisionVertices.push_back(vertex.pos);
	}

	if (vertices.empty()) {
		mesh.localBoundsMin = glm::vec3(-0.5f);
		mesh.localBoundsMax = glm::vec3(0.5f);
	}

	mesh.collisionIndices = indices;

	createVertexBuffer(vertices, commandPool, mesh.vertexBuffer);
	createIndexBuffer(indices, commandPool, mesh.indexBuffer);

	mesh.indexCount = static_cast<uint32_t>(indices.size());

	return mesh;
}

void MeshManager::cleanup() {
	// RAII cleanup via BufferHandle destruction if manager exists
	// But we need to explicitly destroy them here if we want to clear them now
	if (bufferManager) {
		auto destroyMesh = [&](Mesh& mesh) {
			if (mesh.vertexBuffer.isValid()) {
				bufferManager->destroyBuffer(mesh.vertexBuffer);
				mesh.vertexBuffer = {};
			}
			if (mesh.indexBuffer.isValid()) {
				bufferManager->destroyBuffer(mesh.indexBuffer);
				mesh.indexBuffer = {};
			}
		};

		destroyMesh(quad);
		destroyMesh(cube);
		destroyMesh(sphere);
		for (auto& [name, mesh] : importedMeshes) {
			(void)name;
			destroyMesh(mesh);
		}
		importedMeshes.clear();
		importedMeshParts.clear();
	}
}

VkBuffer MeshManager::getVertexBuffer(const Mesh& mesh) {
	if (!bufferManager || !mesh.vertexBuffer.isValid())
		return VK_NULL_HANDLE;
	return static_cast<Renderer::VulkanBuffer*>(bufferManager)->getVulkanBuffer(mesh.vertexBuffer);
}

VkBuffer MeshManager::getIndexBuffer(const Mesh& mesh) {
	if (!bufferManager || !mesh.indexBuffer.isValid())
		return VK_NULL_HANDLE;
	return static_cast<Renderer::VulkanBuffer*>(bufferManager)->getVulkanBuffer(mesh.indexBuffer);
}

void MeshManager::createVertexBuffer(std::vector<Vertex> vertices,
									 VkCommandPool commandPool,
									 Renderer::BufferHandle& vertexBuffer) {
	if (!bufferManager)
		return;

	VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

	// Create staging buffer (CPU -> GPU)
	Renderer::BufferDesc stagingDesc;
	stagingDesc.size = bufferSize;
	stagingDesc.usage = Renderer::BufferUsage::TransferSrc;
	stagingDesc.memory = Renderer::MemoryType::CpuToGpu;

	Renderer::BufferHandle stagingBuffer = bufferManager->createBuffer(stagingDesc);
	bufferManager->updateBuffer(stagingBuffer, vertices.data(), bufferSize);

	// Create GPU-only vertex buffer
	Renderer::BufferDesc vertexDesc;
	vertexDesc.size = bufferSize;
	vertexDesc.usage = Renderer::BufferUsage::Vertex | Renderer::BufferUsage::TransferDst;
	vertexDesc.memory = Renderer::MemoryType::GpuOnly;

	vertexBuffer = bufferManager->createBuffer(vertexDesc);

	// Copy from staging to vertex buffer using Utils (needs VkBuffer handles)
	VkBuffer srcVk = static_cast<Renderer::VulkanBuffer*>(bufferManager)->getVulkanBuffer(stagingBuffer);
	VkBuffer dstVk = static_cast<Renderer::VulkanBuffer*>(bufferManager)->getVulkanBuffer(vertexBuffer);

	Utils::copyBuffer(commandPool, srcVk, dstVk, bufferSize);

	// Cleanup staging buffer
	bufferManager->destroyBuffer(stagingBuffer);
}

void MeshManager::createIndexBuffer(std::vector<uint32_t> indices,
									VkCommandPool commandPool,
									Renderer::BufferHandle& indexBuffer) {
	if (!bufferManager)
		return;

	VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

	// Create staging buffer (CPU -> GPU)
	Renderer::BufferDesc stagingDesc;
	stagingDesc.size = bufferSize;
	stagingDesc.usage = Renderer::BufferUsage::TransferSrc;
	stagingDesc.memory = Renderer::MemoryType::CpuToGpu;

	Renderer::BufferHandle stagingBuffer = bufferManager->createBuffer(stagingDesc);
	bufferManager->updateBuffer(stagingBuffer, indices.data(), bufferSize);

	// Create GPU-only index buffer
	Renderer::BufferDesc indexDesc;
	indexDesc.size = bufferSize;
	indexDesc.usage = Renderer::BufferUsage::Index | Renderer::BufferUsage::TransferDst;
	indexDesc.memory = Renderer::MemoryType::GpuOnly;

	indexBuffer = bufferManager->createBuffer(indexDesc);

	// Copy from staging to index buffer
	VkBuffer srcVk = static_cast<Renderer::VulkanBuffer*>(bufferManager)->getVulkanBuffer(stagingBuffer);
	VkBuffer dstVk = static_cast<Renderer::VulkanBuffer*>(bufferManager)->getVulkanBuffer(indexBuffer);

	Utils::copyBuffer(commandPool, srcVk, dstVk, bufferSize);

	// Cleanup staging buffer
	bufferManager->destroyBuffer(stagingBuffer);
}
