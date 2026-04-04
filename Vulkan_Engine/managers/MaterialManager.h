#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "renderer/RenderTypes.h" // For BufferHandle
#include "vulkan/vulkan_core.h"

#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"

class TextureManager;

namespace Renderer {
class GraphicsBuffer;
class GraphicsResourceBinder;
class VulkanShadowMap;
} // namespace Renderer

// Forward-declare the VulkanIBL data needed for binding
struct VulkanIBLData {
	VkImageView irradianceView;
	VkSampler irradianceSampler;
};

struct MaterialProperties {
	alignas(16) glm::vec4 albedo_pad{1.0f, 1.0f, 1.0f, 0.0f}; // xyz = albedo, w = unused
	alignas(4) float metallic{0.0f};
	alignas(4) float roughness{0.5f};
	alignas(4) float ao{1.0f};
	alignas(4) float _pad{0.0f}; // pad to 32 bytes for std140 (16 + 4*4 = 32)
};

struct Material {
	std::string name;
	std::string filePath;
	std::string albedoTextureKey;
	std::string roughnessTextureKey;
	std::string metallicTextureKey;
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
	createMaterial(const std::string& name,
				   const std::string& albedoTexturePath = "",
				   const std::string& path = "",
				   const std::string& roughnessTexturePath = "",
				   const std::string& metallicTexturePath = "");
	Material* getMaterial(const std::string& filePath);
	const std::unordered_map<std::string, Material*>& getAllMaterials();

	Material* loadMaterialFromFile(const std::string& path);
	void saveMaterialToFile(const std::string& path,
							const std::string& name,
							const std::string& albedoTextureKey = "",
							const std::string& roughnessTextureKey = "",
							const std::string& metallicTextureKey = "",
							const glm::vec3& albedo = glm::vec3(1.0f),
							float metallic = 0.0f,
							float roughness = 0.5f,
							float ao = 1.0f);
	Material* updateMaterialTexture(const std::string& materialPath, const std::string& texturePath);
	Material* updateMaterialRoughnessTexture(const std::string& materialPath, const std::string& texturePath);
	Material* updateMaterialMetallicTexture(const std::string& materialPath, const std::string& texturePath);
	void updateMaterialProperties(Material* material, uint32_t frame);

	// Deferred destruction for descriptor sets
	void cleanupPendingResources(uint32_t frameIndex);

	// Set buffer manager (for deferred initialization)
	void setBufferManager(Renderer::GraphicsBuffer* bufferMgr);
	void setResourceBinder(Renderer::GraphicsResourceBinder* binder);
	void setLightManager(class LightManager* lightMgr);
	void setShadowMap(Renderer::VulkanShadowMap* map);
	void setIrradianceMap(VkImageView view, VkSampler sampler);
	void setSpecularIBL(VkImageView prefilterView,
						VkSampler prefilterSampler,
						VkImageView brdfLutView,
						VkSampler brdfLutSampler);

	VkDescriptorSetLayout getStandardLayout() const;

	// Get VkBuffer for descriptor writes (temporary until descriptor abstraction)
	VkBuffer getMaterialPropertyBuffer(Material* material, uint32_t frame);

private:
	TextureManager& textureManager;
	Renderer::GraphicsBuffer* bufferManager = nullptr;
	Renderer::GraphicsResourceBinder* resourceBinder = nullptr;
	class LightManager* lightManager = nullptr;
	Renderer::VulkanShadowMap* shadowMap = nullptr;
	VkImageView irradianceView = VK_NULL_HANDLE;
	VkSampler irradianceSampler = VK_NULL_HANDLE;
	VkImageView prefilterView = VK_NULL_HANDLE;
	VkSampler prefilterSamplerHandle = VK_NULL_HANDLE;
	VkImageView brdfLutView = VK_NULL_HANDLE;
	VkSampler brdfLutSamplerHandle = VK_NULL_HANDLE;

	Renderer::ResourceSetLayoutHandle standardPbrLayout = {0};

	void destroyMaterialInternal(const std::string& name);
	std::unordered_map<std::string, Material*> materials;
	// VkDescriptorPool descriptorPool = VK_NULL_HANDLE; // Removed
	void createDescriptorSets(const std::string& materialPath);
	void createMaterialPropertyBuffers(Material* material);
	// Deferred destruction queue. Index = frame index.
	std::vector<std::vector<Renderer::ResourceSetHandle>> pendingKill;
};
