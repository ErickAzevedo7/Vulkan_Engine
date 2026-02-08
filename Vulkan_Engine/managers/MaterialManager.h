#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "vulkan/vulkan_core.h"

#include "glm/ext/vector_float3.hpp"


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
	std::vector<VkDescriptorSet> descriptorSets;
	MaterialProperties properties;
	std::vector<VkBuffer> propertyBuffers;
	std::vector<VkDeviceMemory> propertyBufferMemory;
};

class MaterialManager {
public:
	MaterialManager();
	~MaterialManager();

	void loadDefault();
	// Load all material assets from disk at startup
	void loadAllFromAssets();
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

private:
	void createDescriptorPool();
	void destroyMaterialInternal(const std::string& name);
	std::unordered_map<std::string, Material*> materials;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	void createDescriptorSets(const std::string& texturePath, const std::string& materialPath);
	void createMaterialPropertyBuffers(Material* material);
};
