#include "MaterialManager.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "context/ResourceContext.h"
#include "core/vulkancore.h"
#include "managers/LightManager.h"
#include "managers/TextureManager.h"
#include "renderer/GraphicsBuffer.h"
#include "renderer/GraphicsResourceBinder.h" // Added
#include "renderer/RenderTypes.h"
#include "renderer/vulkan/VulkanBuffer.h"
#include "renderer/vulkan/VulkanResourceBinder.h"
#include "renderer/vulkan/VulkanShadowMap.h"
#include "renderer/vulkan/VulkanTexture.h"
#include "vulkan/vulkan_core.h"

#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "nlohmann/json.hpp"
#include "nlohmann/json_fwd.hpp"


// Helper to produce a consistent key for material map lookups (normalizes
// path separators to '/'). If a non-path logical name is used (e.g. "default")
// this returns it unchanged.
static std::string normalizeKey(const std::string& s) {
	try {
		std::filesystem::path p(s);
		return p.generic_string();
	} catch (...) {
		return s;
	}
}

MaterialManager::MaterialManager(TextureManager& textureManager) : textureManager(textureManager) {
	// Constructor
	pendingKill.resize(MAX_FRAMES_IN_FLIGHT);
}

void MaterialManager::init() {
	if (resourceBinder && !standardPbrLayout.isValid()) {
		Renderer::ResourceSetLayoutDesc desc;
		desc.debugName = "StandardPBR";

		Renderer::ResourceBinding texBinding;
		texBinding.binding = 0;
		texBinding.type = Renderer::ResourceType::CombinedTextureSampler;
		texBinding.stages = Renderer::ShaderStage::Fragment;
		desc.bindings.push_back(texBinding);

		Renderer::ResourceBinding propBinding;
		propBinding.binding = 1;
		propBinding.type = Renderer::ResourceType::UniformBuffer;
		propBinding.stages = Renderer::ShaderStage::Fragment;
		desc.bindings.push_back(propBinding);

		standardPbrLayout = resourceBinder->createLayout(desc);
	}
}

MaterialManager::~MaterialManager() {
	// Destructor - automatic cleanup (RAII)
	for (auto& pair : materials) {
		Material* mat = pair.second;
		if (mat) {
			if (bufferManager) {
				// Clean up property buffers
				for (auto& handle : mat->propertyBuffers) {
					if (handle.isValid()) {
						bufferManager->destroyBuffer(handle);
					}
				}
			}

			// Free resource sets
			if (resourceBinder) {
				for (auto& set : mat->resourceSets) {
					resourceBinder->freeSet(set);
				}
			}

			delete mat;
		}
	}
	materials.clear();

	// Destroy standard layout
	if (resourceBinder && standardPbrLayout.isValid()) {
		resourceBinder->destroyLayout(standardPbrLayout);
	}
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
	saveMaterialToFile(defaultMaterialPath, defaultName, textureManager.kDefaultTextureKey);
	createMaterial(defaultName, textureManager.kDefaultTextureKey, defaultMaterialPath);
}

Material* MaterialManager::createMaterial(const std::string& name,
										  const std::string& albedoTexturePath,
										  const std::string& path) {
	std::string albedoKey = albedoTexturePath.empty() ? textureManager.kDefaultTextureKey : albedoTexturePath;
	std::string materialPath = path;
	std::string mapKey = normalizeKey(materialPath);

	// Check if material already exists
	auto it = materials.find(mapKey);
	if (it != materials.end()) {
		std::cerr << "MaterialManager::createMaterial: material already exists for path '" << materialPath << "'"
				  << std::endl;
		return it->second;
	}

	// Create new material
	Material* mat = new Material();
	materials[mapKey] = mat;
	mat->filePath = materialPath;
	mat->name = name;
	mat->albedoTextureKey = albedoKey;

	// Try to load properties from file if it exists
	if (std::filesystem::exists(path)) {
		std::ifstream file(path);
		if (file.is_open()) {
			try {
				nlohmann::json j;
				file >> j;
				file.close();

				// Load PBR material properties if they exist in the file
				if (j.contains("albedo") && j["albedo"].is_array() && j["albedo"].size() == 3) {
					mat->properties.albedo_pad = glm::vec4(
						j["albedo"][0].get<float>(), j["albedo"][1].get<float>(), j["albedo"][2].get<float>(), 0.0f);
				}

				if (j.contains("metallic") && j["metallic"].is_number()) {
					mat->properties.metallic = j["metallic"].get<float>();
				}

				if (j.contains("roughness") && j["roughness"].is_number()) {
					mat->properties.roughness = j["roughness"].get<float>();
				}

				if (j.contains("ao") && j["ao"].is_number()) {
					mat->properties.ao = j["ao"].get<float>();
				}
			} catch (const std::exception& e) {
				std::cerr << "MaterialManager::createMaterial: failed to load properties from file: " << e.what()
						  << std::endl;
			}
		}
	}

	// Create property buffers with loaded/default properties
	createMaterialPropertyBuffers(mat);

	// Create descriptor sets
	createDescriptorSets(albedoKey, materialPath);

	std::cerr << "MaterialManager::createMaterial: created '" << materialPath << "'" << std::endl;
	return mat;
}

Material* MaterialManager::updateMaterialTexture(const std::string& materialPath, const std::string& texturePath) {
	std::string matKey = normalizeKey(materialPath);
	Material* mat = getMaterial(matKey);

	// update texture key and recreate descriptor sets
	mat->albedoTextureKey = texturePath.empty() ? textureManager.kDefaultTextureKey : texturePath;
	createDescriptorSets(mat->albedoTextureKey, materialPath);
	std::cerr << "MaterialManager::updateMaterialTexture: updated material '" << materialPath << "' with texture '"
			  << mat->albedoTextureKey << "'" << std::endl;
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
			std::cerr << "MaterialManager::loadAllFromAssets: found material file: " << p.string() << std::endl;
			loadMaterialFromFile(p.string());
		}
	}
}

Material* MaterialManager::loadMaterialFromFile(const std::string& path) {
	// If material already loaded, return it.
	std::string norm = normalizeKey(path);
	auto it = materials.find(norm);
	if (it != materials.end()) {
		std::cerr << "MaterialManager::loadMaterialFromFile: reusing existing material for path '" << path << "'"
				  << std::endl;
		return it->second;
	}

	std::ifstream file(path);
	if (!file.is_open()) {
		std::cerr << "MaterialManager::loadMaterialFromFile: failed to open file: " << path << std::endl;
		return nullptr;
	}

	try {
		nlohmann::json j;
		file >> j;
		file.close();

		if (!j.contains("name") || !j.contains("albedoTextureKey")) {
			std::cerr << "MaterialManager::loadMaterialFromFile: missing 'name' or 'albedoTextureKey' in file: " << path
					  << std::endl;
			return nullptr;
		}

		std::string name = j["name"].get<std::string>();
		std::string albedoKey = j["albedoTextureKey"].get<std::string>();

		if (name.empty() || albedoKey.empty()) {
			std::cerr << "MaterialManager::loadMaterialFromFile: empty name or albedoTextureKey in file: " << path
					  << std::endl;
			return nullptr;
		}

		albedoKey = std::filesystem::path(albedoKey).generic_string();

		if (albedoKey.empty()) {
			albedoKey = textureManager.kDefaultTextureKey;
		}

		// Check that the texture exists before creating the material
		try {
			textureManager.getTexture(albedoKey);
		} catch (...) {
			std::cerr << "MaterialManager::loadMaterialFromFile: texture key not found: '" << albedoKey
					  << "' for material '" << name << "' in file: " << path << std::endl;
			return nullptr;
		}

		std::cerr << "MaterialManager::loadMaterialFromFile: creating material with name '" << name
				  << "' and file path '" << path << "' using albedoTextureKey '" << albedoKey << "'" << std::endl;

		Material* created = createMaterial(name, albedoKey, path);
		if (created) {
			std::cerr << "MaterialManager: created material for path '" << path << "'" << std::endl;
		} else {
			std::cerr << "MaterialManager: failed to create material for path '" << path << "'" << std::endl;
		}
		return created;
	} catch (...) {
		std::cerr << "MaterialManager::loadMaterialFromFile: exception while reading file: " << path << std::endl;
		return nullptr;
	}
}

void MaterialManager::saveMaterialToFile(const std::string& path,
										 const std::string& name,
										 const std::string& albedoTextureKey,
										 const glm::vec3& albedo,
										 float metallic,
										 float roughness,
										 float ao) {
	std::ofstream file(path, std::ios::trunc);
	if (!file.is_open()) {
		return;
	}

	nlohmann::json j;
	j["name"] = name;
	j["albedoTextureKey"] = albedoTextureKey.empty() ? textureManager.kDefaultTextureKey : albedoTextureKey;

	// Save PBR material properties
	j["albedo"] = {albedo.x, albedo.y, albedo.z};
	j["metallic"] = metallic;
	j["roughness"] = roughness;
	j["ao"] = ao;

	file << j.dump(4) << std::endl;
	file.close();
}

void MaterialManager::destroyMaterialInternal(const std::string& name) {
	auto it = materials.find(name);
	if (it == materials.end()) {
		return;
	}

	Material* mat = it->second;
	if (mat) {
		if (resourceBinder) {
			for (auto& set : mat->resourceSets) {
				resourceBinder->freeSet(set);
			}
		}

		if (bufferManager) {
			for (auto& handle : mat->propertyBuffers) {
				if (handle.isValid()) {
					bufferManager->destroyBuffer(handle);
				}
			}
		}

		delete mat;
	}
	materials.erase(it);
}

void MaterialManager::createDescriptorSets(const std::string& texturePath, const std::string& materialPath) {
	if (!resourceBinder || !bufferManager) {
		std::cerr << "MaterialManager::createDescriptorSets: dependencies not set" << std::endl;
		return;
	}

	// Get existing material from map
	std::string mapKey = normalizeKey(materialPath);
	auto it = materials.find(mapKey);
	if (it == materials.end() || !it->second) {
		std::cerr << "MaterialManager::createDescriptorSets: material not found for path '" << materialPath << "'"
				  << std::endl;
		return;
	}

	Material* material = it->second;

	// Free any existing resource sets
	// Free any existing resource sets (DEFERRED)
	if (!material->resourceSets.empty()) {
		for (size_t i = 0; i < material->resourceSets.size(); i++) {
			if (i < pendingKill.size()) {
				pendingKill[i].push_back(material->resourceSets[i]);
			}
		}
	}

	material->resourceSets.resize(MAX_FRAMES_IN_FLIGHT);

	// Use the official standard PBR layout defined in init()
	Renderer::ResourceSetLayoutHandle layout = standardPbrLayout;
	if (!layout.isValid()) {
		// Fallback for safety, though it should be initialized in init()
		init();
		layout = standardPbrLayout;
	}

	// Allocate and update sets
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		material->resourceSets[i] = resourceBinder->allocateSet(layout);

		// Prepare bindings
		std::vector<Renderer::ResourceBufferBinding> bufferBindings;
		std::vector<Renderer::ResourceImageBinding> imageBindings;

		// Binding 0: Texture
		Texture* texture = nullptr;
		try {
			texture = textureManager.getTexture(texturePath);
		} catch (...) {
			texture = textureManager.getTexture(textureManager.kDefaultTextureKey);
		}

		Renderer::ResourceImageBinding texBinding;
		texBinding.binding = 0;
		texBinding.texture = texture->handle;
		texBinding.sampler = texture->sampler;
		imageBindings.push_back(texBinding);

		// Binding 1: Material Properties
		Renderer::ResourceBufferBinding matBinding;
		matBinding.binding = 1;
		matBinding.buffer = material->propertyBuffers[i]; // BufferHandle
		matBinding.offset = 0;
		matBinding.range = sizeof(MaterialProperties);
		bufferBindings.push_back(matBinding);

		// Update via Binder
		resourceBinder->updateSet(material->resourceSets[i], bufferBindings, imageBindings);
	}
}

VkDescriptorSetLayout MaterialManager::getStandardLayout() const {
	if (resourceBinder && standardPbrLayout.isValid()) {
		auto* vkBinder = static_cast<Renderer::VulkanResourceBinder*>(resourceBinder);
		auto* layoutPtr = static_cast<VkDescriptorSetLayout*>(vkBinder->getNativeLayoutHandle(standardPbrLayout));
		return layoutPtr ? *layoutPtr : VK_NULL_HANDLE;
	}
	return VK_NULL_HANDLE;
}

void MaterialManager::createMaterialPropertyBuffers(Material* material) {
	if (!material || !bufferManager)
		return;

	material->propertyBuffers.resize(MAX_FRAMES_IN_FLIGHT);

	size_t bufferSize = sizeof(MaterialProperties);

	// Create uniform buffers using abstraction
	Renderer::BufferDesc desc;
	desc.size = bufferSize;
	desc.usage = Renderer::BufferUsage::Uniform;
	desc.memory = Renderer::MemoryType::CpuToGpu; // CPU-writable for updates
	desc.debugName = "Material Property Buffer";

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		material->propertyBuffers[i] = bufferManager->createBuffer(desc);

		// Initialize with default properties
		bufferManager->updateBuffer(material->propertyBuffers[i], &material->properties, bufferSize);
	}
}

void MaterialManager::updateMaterialProperties(Material* material, uint32_t frame) {
	if (!material || !bufferManager || frame >= material->propertyBuffers.size())
		return;

	// Update buffer using abstraction
	bufferManager->updateBuffer(material->propertyBuffers[frame], &material->properties, sizeof(MaterialProperties));
}

void MaterialManager::setBufferManager(Renderer::GraphicsBuffer* bufferMgr) {
	bufferManager = bufferMgr;
}

void MaterialManager::setResourceBinder(Renderer::GraphicsResourceBinder* binder) {
	resourceBinder = binder;
}

VkBuffer MaterialManager::getMaterialPropertyBuffer(Material* material, uint32_t frame) {
	if (!material || !bufferManager || frame >= material->propertyBuffers.size())
		return VK_NULL_HANDLE;

	// Cast to VulkanBuffer for Vulkan-specific interop (temporary until descriptor abstraction)
	auto* vulkanBuffer = static_cast<Renderer::VulkanBuffer*>(bufferManager);
	return vulkanBuffer->getVulkanBuffer(material->propertyBuffers[frame]);
}

void MaterialManager::setLightManager(LightManager* lightMgr) {
	lightManager = lightMgr;
}

void MaterialManager::setShadowMap(Renderer::VulkanShadowMap* map) {
	shadowMap = map;

	if (shadowMap && resourceBinder) {
		for (auto& pair : materials) {
			Material* material = pair.second;
			for (size_t i = 0; i < material->resourceSets.size(); i++) {
				VkDescriptorSet vkSet =
					*static_cast<VkDescriptorSet*>(resourceBinder->getNativeHandle(material->resourceSets[i]));

				std::vector<VkWriteDescriptorSet> manualWrites;

				VkDescriptorImageInfo shadowInfo{};
				VkWriteDescriptorSet shadowWrite{};
				shadowInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				shadowInfo.imageView = static_cast<VkImageView>(shadowMap->getDepth2DView());
				shadowInfo.sampler = static_cast<VkSampler>(shadowMap->getDepthSampler());

				shadowWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				shadowWrite.dstSet = vkSet;
				shadowWrite.dstBinding = 4;
				shadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				shadowWrite.descriptorCount = 1;
				shadowWrite.pImageInfo = &shadowInfo;
				manualWrites.push_back(shadowWrite);

				VkDescriptorImageInfo shadowCubeInfo{};
				VkWriteDescriptorSet shadowCubeWrite{};
				shadowCubeInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
				shadowCubeInfo.imageView = static_cast<VkImageView>(shadowMap->getDepthCubeImageView());
				shadowCubeInfo.sampler = static_cast<VkSampler>(shadowMap->getDepthSampler());

				shadowCubeWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				shadowCubeWrite.dstSet = vkSet;
				shadowCubeWrite.dstBinding = 5;
				shadowCubeWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
				shadowCubeWrite.descriptorCount = 1;
				shadowCubeWrite.pImageInfo = &shadowCubeInfo;
				manualWrites.push_back(shadowCubeWrite);

				if (!manualWrites.empty()) {
					vkUpdateDescriptorSets(VulkanCore::getDevice(),
										   static_cast<uint32_t>(manualWrites.size()),
										   manualWrites.data(),
										   0,
										   nullptr);
				}
			}
		}
	}
}

void MaterialManager::setIrradianceMap(VkImageView view, VkSampler sampler) {
	irradianceView = view;
	irradianceSampler = sampler;

	if (irradianceView == VK_NULL_HANDLE || !resourceBinder)
		return;

	// Retroactively update binding 6 on every existing material descriptor set
	for (auto& pair : materials) {
		Material* material = pair.second;
		for (size_t i = 0; i < material->resourceSets.size(); i++) {
			VkDescriptorSet vkSet =
				*static_cast<VkDescriptorSet*>(resourceBinder->getNativeHandle(material->resourceSets[i]));

			VkDescriptorImageInfo irrInfo{};
			irrInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			irrInfo.imageView = irradianceView;
			irrInfo.sampler = irradianceSampler;

			VkWriteDescriptorSet irrWrite{};
			irrWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			irrWrite.dstSet = vkSet;
			irrWrite.dstBinding = 6;
			irrWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			irrWrite.descriptorCount = 1;
			irrWrite.pImageInfo = &irrInfo;

			vkUpdateDescriptorSets(VulkanCore::getDevice(), 1, &irrWrite, 0, nullptr);
		}
	}
}

void MaterialManager::cleanupPendingResources(uint32_t frameIndex) {
	if (frameIndex >= pendingKill.size() || !resourceBinder)
		return;

	auto& queue = pendingKill[frameIndex];
	for (auto& set : queue) {
		resourceBinder->freeSet(set);
	}
	queue.clear();
}

void MaterialManager::setSpecularIBL(VkImageView pView, VkSampler pSampler, VkImageView bView, VkSampler bSampler) {
	prefilterView = pView;
	prefilterSamplerHandle = pSampler;
	brdfLutView = bView;
	brdfLutSamplerHandle = bSampler;

	if (!resourceBinder)
		return;

	// Retroactively update bindings 7 and 8 on every existing material descriptor set
	for (auto& pair : materials) {
		Material* material = pair.second;
		for (size_t i = 0; i < material->resourceSets.size(); i++) {
			VkDescriptorSet vkSet =
				*static_cast<VkDescriptorSet*>(resourceBinder->getNativeHandle(material->resourceSets[i]));

			VkDescriptorImageInfo prefilterInfo{};
			prefilterInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			prefilterInfo.imageView = prefilterView;
			prefilterInfo.sampler = prefilterSamplerHandle;

			VkWriteDescriptorSet prefilterWrite{};
			prefilterWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			prefilterWrite.dstSet = vkSet;
			prefilterWrite.dstBinding = 7;
			prefilterWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			prefilterWrite.descriptorCount = 1;
			prefilterWrite.pImageInfo = &prefilterInfo;

			VkDescriptorImageInfo brdfInfo{};
			brdfInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			brdfInfo.imageView = brdfLutView;
			brdfInfo.sampler = brdfLutSamplerHandle;

			VkWriteDescriptorSet brdfWrite{};
			brdfWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			brdfWrite.dstSet = vkSet;
			brdfWrite.dstBinding = 8;
			brdfWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			brdfWrite.descriptorCount = 1;
			brdfWrite.pImageInfo = &brdfInfo;

			VkWriteDescriptorSet writes[] = {prefilterWrite, brdfWrite};
			vkUpdateDescriptorSets(VulkanCore::getDevice(), 2, writes, 0, nullptr);
		}
	}
}
