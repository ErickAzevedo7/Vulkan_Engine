#include "MaterialManager.h"
#include "managers/LightManager.h"

// initialize static members
std::unordered_map<std::string, Material*> MaterialManager::materials;
VkDescriptorPool MaterialManager::descriptorPool = VK_NULL_HANDLE;

// Helper to produce a consistent key for material map lookups (normalizes
// path separators to '/'). If a non-path logical name is used (e.g. "default")
// this returns it unchanged.
static std::string normalizeKey(const std::string& s) {
	try {
		std::filesystem::path p(s);
		return p.generic_string();
	}
	catch (...) {
		return s;
	}
}

void MaterialManager::init() {
	createDescriptorPool();
	loadAllFromAssets();
}

void MaterialManager::loadDefault() {
	// Ensure there is a default material stored under a material path.
	// If a default material file exists on disk, load it; otherwise create
	// one that uses the engine default texture and save it.
	const std::string defaultMaterialPath = "common/material/default.mat";

	if (std::filesystem::exists(defaultMaterialPath)) {
		loadMaterialFromFile(defaultMaterialPath);
		return;
	}

	const std::string defaultName = "default";
	saveMaterialToFile(defaultMaterialPath, defaultName, TextureManager::kDefaultTextureKey);
	createMaterial(defaultName, TextureManager::kDefaultTextureKey, defaultMaterialPath);
}

Material* MaterialManager::createMaterial(const std::string& name,
                                          const std::string& albedoTexturePath,
                                          const std::string& path) {
	std::string albedoKey = albedoTexturePath.empty() ? TextureManager::kDefaultTextureKey : albedoTexturePath;

	std::string materialPath = path;

	createDescriptorSets(albedoKey, materialPath);

	Material* mat = getMaterial(materialPath);
	if (!mat) {
		std::cerr << "MaterialManager::createMaterial: ERROR material not found after createDescriptorSets for '" <<
			materialPath << "'" << std::endl;
		return nullptr;
	}

	mat->name = name;

	std::cerr << "MaterialManager::createMaterial: created '" << materialPath << "'" << std::endl;
	return mat;
}

Material* MaterialManager::updateMaterialTexture(const std::string& materialPath, const std::string& texturePath) {
	std::string matKey = normalizeKey(materialPath);
	Material* mat = getMaterial(matKey);

	// update texture key and recreate descriptor sets
	mat->albedoTextureKey = texturePath.empty() ? TextureManager::kDefaultTextureKey : texturePath;
	createDescriptorSets(mat->albedoTextureKey, materialPath);
	std::cerr << "MaterialManager::updateMaterialTexture: updated material '" << materialPath << "' with texture '" << mat
		->albedoTextureKey << "'" << std::endl;
	return mat;
}

Material* MaterialManager::getMaterial(const std::string& filePath) {
	// Normalize path keys to generic format so lookups are consistent
	std::string key = normalizeKey(filePath);
	auto it = materials.find(key);
	if (it != materials.end()) {
		return it->second;
	}
	return nullptr;
}

const std::unordered_map<std::string, Material*>& MaterialManager::getAllMaterials() {
	return materials;
}

void MaterialManager::loadAllFromAssets() {
	namespace fs = std::filesystem;
	const fs::path materialsRoot = fs::path("assets");

	if (!fs::exists(materialsRoot) || !fs::is_directory(materialsRoot)) {
		std::cerr << "MaterialManager::loadAllFromAssets: assets directory not found or not a directory: "
			<< materialsRoot.string() << std::endl;
		return;
	}

	for (const auto& entry : fs::recursive_directory_iterator(materialsRoot)) {
		if (!entry.is_regular_file()) {
			continue;
		}

		fs::path p = entry.path();
		std::string ext = p.extension().string();
		for (auto& c : ext) {
			c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
		}

		if (ext == ".mat") {
			std::cerr << "MaterialManager::loadAllFromAssets: found material file: "
				<< p.string() << std::endl;
			loadMaterialFromFile(p.string());
		}
	}
}

Material* MaterialManager::loadMaterialFromFile(const std::string& path) {
	// If material already loaded, return it.
	std::string norm = normalizeKey(path);
	auto it = materials.find(norm);
	if (it != materials.end()) {
		std::cerr << "MaterialManager::loadMaterialFromFile: reusing existing material for path '" << path << "'" <<
			std::endl;
		return it->second;
	}

	std::ifstream file(path);
	if (!file.is_open()) {
		std::cerr << "MaterialManager::loadMaterialFromFile: failed to open file: "
			<< path << std::endl;
		return nullptr;
	}

	try {
		nlohmann::json j;
		file >> j;
		file.close();

		if (!j.contains("name") || !j.contains("albedoTextureKey")) {
			std::cerr << "MaterialManager::loadMaterialFromFile: missing 'name' or 'albedoTextureKey' in file: "
				<< path << std::endl;
			return nullptr;
		}

		std::string name = j["name"].get<std::string>();
		std::string albedoKey = j["albedoTextureKey"].get<std::string>();

		if (name.empty() || albedoKey.empty()) {
			std::cerr << "MaterialManager::loadMaterialFromFile: empty name or albedoTextureKey in file: "
				<< path << std::endl;
			return nullptr;
		}

		if (albedoKey.empty()) {
			albedoKey = TextureManager::kDefaultTextureKey;
		}

		// Check that the texture exists before creating the material
		try {
			TextureManager::getTexture(albedoKey);
		}
		catch (...) {
			std::cerr << "MaterialManager::loadMaterialFromFile: texture key not found: '"
				<< albedoKey << "' for material '" << name
				<< "' in file: " << path << std::endl;
			return nullptr;
		}

		std::cerr << "MaterialManager::loadMaterialFromFile: creating material with name '"
			<< name << "' and file path '" << path << "' using albedoTextureKey '" << albedoKey
			<< "'" << std::endl;

		Material* created = createMaterial(name, albedoKey, path);
		if (created) {
			std::cerr << "MaterialManager: created material for path '" << path << "'" << std::endl;
		}
		else {
			std::cerr << "MaterialManager: failed to create material for path '" << path << "'" << std::endl;
		}
		return created;
	}
	catch (...) {
		std::cerr << "MaterialManager::loadMaterialFromFile: exception while reading file: "
			<< path << std::endl;
		return nullptr;
	}
}

void MaterialManager::saveMaterialToFile(const std::string& path,
                                         const std::string& name,
                                         const std::string& albedoTextureKey) {
	std::ofstream file(path, std::ios::trunc);
	if (!file.is_open()) {
		return;
	}

	nlohmann::json j;
	j["name"] = name;
	j["albedoTextureKey"] = albedoTextureKey.empty() ? TextureManager::kDefaultTextureKey : albedoTextureKey;

	file << j.dump(4) << std::endl;
	file.close();
}

void MaterialManager::cleanup() {
	for (auto& pair : materials) {
		delete pair.second;
	}
	materials.clear();
	vkDestroyDescriptorPool(VulkanCore::getDevice(), descriptorPool, nullptr);
}

void MaterialManager::destroyMaterialInternal(const std::string& name) {
	auto it = materials.find(name);
	if (it == materials.end()) {
		return;
	}

	Material* mat = it->second;
	if (mat) {
		if (!mat->descriptorSets.empty() && descriptorPool != VK_NULL_HANDLE) {
			vkFreeDescriptorSets(
				VulkanCore::getDevice(),
				descriptorPool,
				static_cast<uint32_t>(mat->descriptorSets.size()),
				mat->descriptorSets.data());
		}
		delete mat;
	}
	materials.erase(it);
}

void MaterialManager::createDescriptorPool() {
	// Allow many materials; each material needs MAX_FRAMES_IN_FLIGHT sets.
	const uint32_t maxMaterials = 100; // adjust as needed
	const uint32_t totalSets = maxMaterials * MAX_FRAMES_IN_FLIGHT;

	std::array<VkDescriptorPoolSize, 3> poolSizes{};
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	poolSizes[0].descriptorCount = totalSets;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[1].descriptorCount = totalSets;
	poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[2].descriptorCount = totalSets;


	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = totalSets;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	if (vkCreateDescriptorPool(VulkanCore::getDevice(), &poolInfo, nullptr,
	                           &descriptorPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create descriptor pool!");
	}
}

void MaterialManager::createDescriptorSets(const std::string& texturePath, const std::string& materialPath) {
	// Preserve existing filePath if this materialy this already exists
	std::string existingPath;
	std::string mapKey = normalizeKey(materialPath);
	auto itExisting = materials.find(mapKey);
	Material* material = nullptr;
	if (itExisting != materials.end() && itExisting->second) {
		material = itExisting->second;
		existingPath = material->filePath;
		// Free any existing descriptor sets for this material before reallocating.
		if (!material->descriptorSets.empty() && descriptorPool != VK_NULL_HANDLE) {
			vkFreeDescriptorSets(VulkanCore::getDevice(), descriptorPool,
			                     static_cast<uint32_t>(material->descriptorSets.size()),
			                     material->descriptorSets.data());
		}
	}
	else {
		material = new Material();
		materials[mapKey] = material;
	}

	material->filePath = existingPath.empty() ? materialPath : existingPath;
	material->albedoTextureKey = texturePath;
	material->descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);


	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT,
	                                           VulkanCore::getDescriptorSetLayout());
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();

	if (vkAllocateDescriptorSets(VulkanCore::getDevice(), &allocInfo,
	                             material->descriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate descriptor sets!");
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = VulkanCore::getUniformBuffers()[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		Texture* texture = nullptr;
		try {
			texture = TextureManager::getTexture(texturePath);
		}
		catch (const std::exception& e) {
			std::cerr << "MaterialManager::createDescriptorSets: texture '" << texturePath <<
				"' not found, falling back to default: " << e.what() << std::endl;
			try {
				texture = TextureManager::getTexture(TextureManager::kDefaultTextureKey);
			}
			catch (...) {
				std::cerr << "MaterialManager::createDescriptorSets: failed to get default texture '" <<
					TextureManager::kDefaultTextureKey << "'" << std::endl;
				throw;
			}
		}
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = texture->imageView;
		imageInfo.sampler = texture->sampler;

		std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = material->descriptorSets[i];
		descriptorWrites[0].dstBinding = 0;
		descriptorWrites[0].dstArrayElement = 0;
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;
		descriptorWrites[0].pImageInfo = nullptr; // Optional
		descriptorWrites[0].pTexelBufferView = nullptr; // Optional

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = material->descriptorSets[i];
		descriptorWrites[1].dstBinding = 1;
		descriptorWrites[1].dstArrayElement = 0;
		descriptorWrites[1].descriptorType =
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;

		vkUpdateDescriptorSets(VulkanCore::getDevice(),
		                       static_cast<uint32_t>(descriptorWrites.size()),
		                       descriptorWrites.data(), 0, nullptr);
	}
}
