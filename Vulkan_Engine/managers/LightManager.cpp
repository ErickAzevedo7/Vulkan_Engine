#include "LightManager.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "components/LightComponent.h"
#include "core/vulkancore.h"
#include "renderer/GraphicsBuffer.h" // Use interface, not VulkanBuffer
#include "renderer/RenderTypes.h"
#include "renderer/vulkan/VulkanBuffer.h" // Only for getVulkanBuffer()
#include "renderer/vulkan/VulkanShadowMap.h"
#include "vulkan/vulkan_core.h"

// light manager: creates a uniform buffer per-frame and exposes
// it so descriptor sets can bind it at binding 2.

LightManager::LightManager(Renderer::GraphicsBuffer* bufferMgr) : bufferManager(bufferMgr) {
	// Constructor - store buffer manager reference (interface pointer)
}

void LightManager::init() {
	// Initialize all resources
	uint32_t count = MAX_FRAMES_IN_FLIGHT;
	lightBuffers.resize(count);

	// Calculate buffer size based on the struct
	size_t bufferSize = sizeof(LightComponent::LightUniform);

	// Create uniform buffers using abstraction
	Renderer::BufferDesc desc;
	desc.size = bufferSize;
	desc.usage = Renderer::BufferUsage::Uniform;
	desc.memory = Renderer::MemoryType::CpuToGpu; // CPU-writable for updates
	desc.debugName = "Light Uniform Buffer";

	for (uint32_t i = 0; i < count; ++i) {
		lightBuffers[i] = bufferManager->createBuffer(desc);
	}
}

LightManager::~LightManager() {
	// Destructor - cleanup using abstraction (RAII)
	for (auto& handle : lightBuffers) {
		if (handle.isValid()) {
			bufferManager->destroyBuffer(handle);
		}
	}

	if (device != VK_NULL_HANDLE && descriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	}
}

void LightManager::updateLight(uint32_t frame, const LightComponent::LightUniform& u) {
	if (frame >= lightBuffers.size())
		return;

	// Update buffer using abstraction
	bufferManager->updateBuffer(lightBuffers[frame], &u, sizeof(LightComponent::LightUniform));
}

VkBuffer LightManager::getLightBuffer(uint32_t frame) {
	// Return underlying Vulkan buffer for descriptor writes
	// Cast to VulkanBuffer for Vulkan-specific interop (temporary until descriptor abstraction)
	auto* vulkanBuffer = static_cast<Renderer::VulkanBuffer*>(bufferManager);
	return vulkanBuffer->getVulkanBuffer(lightBuffers.at(frame));
}

void LightManager::setBufferManager(Renderer::GraphicsBuffer* bufferMgr) {
	bufferManager = bufferMgr;
}

Renderer::BufferHandle LightManager::getLightBufferHandle(uint32_t frame) const {
	if (frame >= lightBuffers.size())
		return {};
	return lightBuffers[frame];
}

void LightManager::initDescriptorResources(VkDevice dev, VkDescriptorPool pool) {
	device = dev;
	descriptorPool = pool;
	createDescriptorSetLayout();
	createDescriptorSets();
}

void LightManager::createDescriptorSetLayout() {
	if (device == VK_NULL_HANDLE)
		return;

	std::vector<VkDescriptorSetLayoutBinding> bindings;

	// 0: Light buffer
	VkDescriptorSetLayoutBinding lightLayoutBinding{};
	lightLayoutBinding.binding = 0;
	lightLayoutBinding.descriptorCount = 1;
	lightLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	lightLayoutBinding.pImmutableSamplers = nullptr;
	lightLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings.push_back(lightLayoutBinding);

	// 1: Shadow Map
	VkDescriptorSetLayoutBinding shadowMapLayoutBinding{};
	shadowMapLayoutBinding.binding = 1;
	shadowMapLayoutBinding.descriptorCount = 1;
	shadowMapLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowMapLayoutBinding.pImmutableSamplers = nullptr;
	shadowMapLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings.push_back(shadowMapLayoutBinding);

	// 2: Shadow Cube Map
	VkDescriptorSetLayoutBinding shadowCubeMapLayoutBinding{};
	shadowCubeMapLayoutBinding.binding = 2;
	shadowCubeMapLayoutBinding.descriptorCount = 1;
	shadowCubeMapLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowCubeMapLayoutBinding.pImmutableSamplers = nullptr;
	shadowCubeMapLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings.push_back(shadowCubeMapLayoutBinding);

	// 3: IBL Irradiance Cubemap
	VkDescriptorSetLayoutBinding irradianceMapLayoutBinding{};
	irradianceMapLayoutBinding.binding = 3;
	irradianceMapLayoutBinding.descriptorCount = 1;
	irradianceMapLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	irradianceMapLayoutBinding.pImmutableSamplers = nullptr;
	irradianceMapLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings.push_back(irradianceMapLayoutBinding);

	// 4: IBL Pre-filtered Specular Cubemap
	VkDescriptorSetLayoutBinding prefilterMapLayoutBinding{};
	prefilterMapLayoutBinding.binding = 4;
	prefilterMapLayoutBinding.descriptorCount = 1;
	prefilterMapLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	prefilterMapLayoutBinding.pImmutableSamplers = nullptr;
	prefilterMapLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings.push_back(prefilterMapLayoutBinding);

	// 5: BRDF Integration LUT
	VkDescriptorSetLayoutBinding brdfLutLayoutBinding{};
	brdfLutLayoutBinding.binding = 5;
	brdfLutLayoutBinding.descriptorCount = 1;
	brdfLutLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	brdfLutLayoutBinding.pImmutableSamplers = nullptr;
	brdfLutLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings.push_back(brdfLutLayoutBinding);

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("LightManager: failed to create lighting descriptor set layout!");
	}
}

void LightManager::createDescriptorSets() {
	if (device == VK_NULL_HANDLE || descriptorPool == VK_NULL_HANDLE || descriptorSetLayout == VK_NULL_HANDLE) {
		return;
	}

	descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();

	if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("LightManager: failed to allocate lighting descriptor sets!");
	}
}

void LightManager::updateDescriptorSets(Renderer::VulkanShadowMap* shadowMap,
										VkImageView irradianceMap,
										VkSampler irradianceSampler,
										VkImageView prefilterMap,
										VkSampler prefilterSampler,
										VkImageView brdfLut,
										VkSampler brdfLutSampler) {
	if (device == VK_NULL_HANDLE || descriptorSets.empty())
		return;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		std::array<VkWriteDescriptorSet, 6> writes{};

		// 0: Light buffer
		VkDescriptorBufferInfo lightInfo{};
		lightInfo.buffer = getLightBuffer(i);
		lightInfo.offset = 0;
		lightInfo.range = sizeof(LightComponent::LightUniform);

		writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[0].dstSet = descriptorSets[i];
		writes[0].dstBinding = 0;
		writes[0].dstArrayElement = 0;
		writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writes[0].descriptorCount = 1;
		writes[0].pBufferInfo = &lightInfo;

		// 1: Shadow Map
		VkDescriptorImageInfo shadow2DInfo{};
		shadow2DInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		shadow2DInfo.imageView = static_cast<VkImageView>(shadowMap->getDepth2DView());
		shadow2DInfo.sampler = static_cast<VkSampler>(shadowMap->getDepthSampler());

		writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[1].dstSet = descriptorSets[i];
		writes[1].dstBinding = 1;
		writes[1].dstArrayElement = 0;
		writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[1].descriptorCount = 1;
		writes[1].pImageInfo = &shadow2DInfo;

		// 2: Shadow Cube Map
		VkDescriptorImageInfo shadowCubeInfo{};
		shadowCubeInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
		shadowCubeInfo.imageView = static_cast<VkImageView>(shadowMap->getDepthCubeImageView());
		shadowCubeInfo.sampler = static_cast<VkSampler>(shadowMap->getDepthSampler());

		writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[2].dstSet = descriptorSets[i];
		writes[2].dstBinding = 2;
		writes[2].dstArrayElement = 0;
		writes[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[2].descriptorCount = 1;
		writes[2].pImageInfo = &shadowCubeInfo;

		// 3: Irradiance Map
		VkDescriptorImageInfo irrInfo{};
		irrInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		irrInfo.imageView = irradianceMap;
		irrInfo.sampler = irradianceSampler;

		writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[3].dstSet = descriptorSets[i];
		writes[3].dstBinding = 3;
		writes[3].dstArrayElement = 0;
		writes[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[3].descriptorCount = 1;
		writes[3].pImageInfo = &irrInfo;

		// 4: Prefilter Map
		VkDescriptorImageInfo prefInfo{};
		prefInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		prefInfo.imageView = prefilterMap;
		prefInfo.sampler = prefilterSampler;

		writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[4].dstSet = descriptorSets[i];
		writes[4].dstBinding = 4;
		writes[4].dstArrayElement = 0;
		writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[4].descriptorCount = 1;
		writes[4].pImageInfo = &prefInfo;

		// 5: BRDF LUT
		VkDescriptorImageInfo brdfInfo{};
		brdfInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		brdfInfo.imageView = brdfLut;
		brdfInfo.sampler = brdfLutSampler;

		writes[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writes[5].dstSet = descriptorSets[i];
		writes[5].dstBinding = 5;
		writes[5].dstArrayElement = 0;
		writes[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writes[5].descriptorCount = 1;
		writes[5].pImageInfo = &brdfInfo;

		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
	}
}
