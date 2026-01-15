#pragma once
#include "managers/TextureManager.h"

struct Material {
	std::string name;
	Texture* albedoTexture;
	std::vector<VkDescriptorSet> descriptorSets;
};

class MaterialManager {
public:
	static void init();
	static void loadDefault();
	static Material* createMaterial(const std::string& name,
	                                const std::string& albedoTexturePath);
	static Material* getMaterial(const std::string& name);
	static const std::unordered_map<std::string, Material*>& getAllMaterials();
	static void cleanup();

private:
	static void createDescriptorPool();
	static std::unordered_map<std::string, Material*> materials;
	static VkDescriptorPool descriptorPool;
	static void createDescriptorSets(std::string path, std::string name);
};
