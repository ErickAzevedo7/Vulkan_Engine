#pragma once

#include "vulkan/vulkan_core.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

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
	static void init();
	static void loadDefault();
	// Load all material assets from disk at startup
	static void loadAllFromAssets();
	static Material*
	createMaterial(const std::string& name, const std::string& albedoTexturePath = "", const std::string& path = "");
	static Material* getMaterial(const std::string& filePath);
	static const std::unordered_map<std::string, Material*>& getAllMaterials();

	static Material* loadMaterialFromFile(const std::string& path);
	static void saveMaterialToFile(const std::string& path,
								   const std::string& name,
								   const std::string& albedoTextureKey = "",
								   const glm::vec3& ambient = glm::vec3(0.15f),
								   float shininess = 32.0f,
								   const glm::vec3& specular = glm::vec3(0.5f),
								   const glm::vec3& diffuse = glm::vec3(0.8f));
	static Material* updateMaterialTexture(const std::string& materialPath, const std::string& texturePath);
	static void updateMaterialProperties(Material* material, uint32_t frame);
	static void cleanup();

private:
	static void createDescriptorPool();
	static void destroyMaterialInternal(const std::string& name);
	static std::unordered_map<std::string, Material*> materials;
	static VkDescriptorPool descriptorPool;
	static void createDescriptorSets(const std::string& texturePath, const std::string& materialPath);
	static void createMaterialPropertyBuffers(Material* material);
};
