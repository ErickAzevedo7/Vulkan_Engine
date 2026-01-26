#pragma once
#include "managers/TextureManager.h"
#include <fstream>
#include <nlohmann/json.hpp>

struct Material {
	std::string name;
	std::string filePath;
	Texture* albedoTexture;
	std::vector<VkDescriptorSet> descriptorSets;
};

class MaterialManager {
public:
	static void init();
	static void loadDefault();
	// Load all material assets from disk at startup
  static void loadAllFromAssets();
	static Material* createMaterial(const std::string& name,
	                                const std::string& albedoTexturePath,
	                                const std::string& path = "");
	static Material* getMaterial(const std::string& name);
	static const std::unordered_map<std::string, Material*>& getAllMaterials();

	static Material* loadMaterialFromFile(const std::string& path);
	static void saveMaterialToFile(const std::string& path,
	                               const std::string& name,
	                               const std::string& albedoTextureKey = "default");
	static void cleanup();

private:
	static void createDescriptorPool();
	static void destroyMaterialInternal(const std::string& name);
	static std::unordered_map<std::string, Material*> materials;
	static VkDescriptorPool descriptorPool;
	static void createDescriptorSets(std::string path, std::string name);
};
