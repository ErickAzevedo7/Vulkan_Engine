#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "renderer/RenderTypes.h" // For BufferHandle
#include "vulkan/vulkan_core.h"

#include "glm/ext/vector_float3.hpp"

class TextureManager;

// Forward declaration - use interface, not implementation
namespace Renderer {
class GraphicsBuffer;
class GraphicsResourceBinder;
} // namespace Renderer

struct MaterialProperties {
	alignas(16) glm::vec3 ambient{0.15f, 0.15f, 0.15f};
	alignas(4) float shininess{32.0f};
	alignas(16) glm::vec3 specular{0.5f, 0.5f, 0.5f};
	alignas(16) glm::vec3 diffuse{0.8f, 0.8f, 0.8f};
};

struct Material {
	std::string name;
	std::string filePath;
	std::string albedoTextureKey;
	// Refactored to use ResourceSetHandle
	std::vector<Renderer::ResourceSetHandle> resourceSets;
	MaterialProperties properties;
	std::vector<Renderer::BufferHandle> propertyBuffers;
};

class MaterialManager {
public:
	MaterialManager(TextureManager& textureManager);
	~MaterialManager();

	void init();
	void loadDefault();
	// Load all material assets from disk at startup
	void loadAllFromAssets();

	// ... existing methods ...
	Material*
	createMaterial(const std::string& name, const std::string& albedoTexturePath = "", const std::string& path = "");
	Material* getMaterial(const std::string& filePath);
	const std::unordered_map<std::string, Material*>& getAllMaterials();

	Material* loadMaterialFromFile(const std::string& path);
	void saveMaterialToFile(const std::string& path,
							const std::string& name,
							const std::string& albedoTextureKey = "",
							const glm::vec3& ambient = glm::vec3(0.15f),
							float shininess = 32.0f,
							const glm::vec3& specular = glm::vec3(0.5f),
							const glm::vec3& diffuse = glm::vec3(0.8f));
	Material* updateMaterialTexture(const std::string& materialPath, const std::string& texturePath);
	void updateMaterialProperties(Material* material, uint32_t frame);
	
	// Deferred destruction for descriptor sets
	void cleanupPendingResources(uint32_t frameIndex);

	// Set buffer manager (for deferred initialization)
	void setBufferManager(Renderer::GraphicsBuffer* bufferMgr);
	void setResourceBinder(Renderer::GraphicsResourceBinder* binder);
	void setLightManager(class LightManager* lightMgr);

	// Get VkBuffer for descriptor writes (temporary until descriptor abstraction)
	VkBuffer getMaterialPropertyBuffer(Material* material, uint32_t frame);

private:
	TextureManager& textureManager;
	Renderer::GraphicsBuffer* bufferManager = nullptr;
	Renderer::GraphicsResourceBinder* resourceBinder = nullptr;
	class LightManager* lightManager = nullptr;

	// Layout key -> Handle map (to reuse layouts)
	std::unordered_map<std::string, Renderer::ResourceSetLayoutHandle> layoutCache;

	void destroyMaterialInternal(const std::string& name);
	std::unordered_map<std::string, Material*> materials;
	// VkDescriptorPool descriptorPool = VK_NULL_HANDLE; // Removed
	void createDescriptorSets(const std::string& texturePath, const std::string& materialPath);
	void createMaterialPropertyBuffers(Material* material);
	// Deferred destruction queue. Index = frame index.
	std::vector<std::vector<Renderer::ResourceSetHandle>> pendingKill;
};
