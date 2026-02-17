#include "VulkanResourceBinder.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "renderer/GraphicsBuffer.h"
#include "renderer/RenderTypes.h"
#include "vulkan/vulkan_core.h"
#include "VulkanBuffer.h"

namespace Renderer {

VulkanResourceBinder::~VulkanResourceBinder() {
	shutdown();
}

void VulkanResourceBinder::initialize(VkDevice device_, GraphicsBuffer* bufferMgr) {
	device = device_;
	bufferManager = bufferMgr;
}

void VulkanResourceBinder::shutdown() {
	// Free all descriptor sets
	if (pool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
		std::vector<VkDescriptorSet> allSets;
		allSets.reserve(sets.size());
		for (const auto& pair : sets) {
			allSets.push_back(pair.second);
		}
		if (!allSets.empty()) {
			vkFreeDescriptorSets(device, pool, static_cast<uint32_t>(allSets.size()), allSets.data());
		}
	}
	sets.clear();

	// Destroy all layouts
	for (const auto& pair : layouts) {
		if (pair.second != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(device, pair.second, nullptr);
		}
	}
	layouts.clear();
	layoutDescs.clear();

	// Destroy pool
	if (pool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, pool, nullptr);
		pool = VK_NULL_HANDLE;
	}

	device = VK_NULL_HANDLE;
	bufferManager = nullptr;
}

void VulkanResourceBinder::createPool(uint32_t maxSets, const std::vector<VkDescriptorPoolSize>& poolSizes) {
	if (device == VK_NULL_HANDLE) {
		throw std::runtime_error("VulkanResourceBinder: device not initialized");
	}

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = maxSets;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
		throw std::runtime_error("VulkanResourceBinder: failed to create descriptor pool");
	}
}

ResourceSetLayoutHandle VulkanResourceBinder::createLayout(const ResourceSetLayoutDesc& desc) {
	if (device == VK_NULL_HANDLE) {
		throw std::runtime_error("VulkanResourceBinder: device not initialized");
	}

	// Convert bindings to Vulkan format
	std::vector<VkDescriptorSetLayoutBinding> vkBindings;
	vkBindings.reserve(desc.bindings.size());

	for (const auto& binding : desc.bindings) {
		VkDescriptorSetLayoutBinding vkBinding{};
		vkBinding.binding = binding.binding;
		vkBinding.descriptorType = toVulkanDescriptorType(binding.type);
		vkBinding.descriptorCount = binding.count;
		vkBinding.stageFlags = toVulkanShaderStages(binding.stages);
		vkBinding.pImmutableSamplers = nullptr;

		vkBindings.push_back(vkBinding);
	}

	// Create layout
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
	layoutInfo.pBindings = vkBindings.data();

	VkDescriptorSetLayout vkLayout;
	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &vkLayout) != VK_SUCCESS) {
		throw std::runtime_error("VulkanResourceBinder: failed to create descriptor set layout");
	}

	// Store layout and return handle
	uint64_t id = nextLayoutId++;
	layouts[id] = vkLayout;
	layoutDescs[id] = desc;

	ResourceSetLayoutHandle handle;
	handle.id = id;
	return handle;
}

void VulkanResourceBinder::destroyLayout(ResourceSetLayoutHandle layout) {
	if (!layout.isValid())
		return;

	auto it = layouts.find(layout.id);
	if (it != layouts.end()) {
		if (it->second != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
			vkDestroyDescriptorSetLayout(device, it->second, nullptr);
		}
		layouts.erase(it);
	}

	layoutDescs.erase(layout.id);
}

ResourceSetHandle VulkanResourceBinder::allocateSet(ResourceSetLayoutHandle layout) {
	if (!layout.isValid()) {
		throw std::runtime_error("VulkanResourceBinder: invalid layout handle");
	}
	if (pool == VK_NULL_HANDLE) {
		throw std::runtime_error("VulkanResourceBinder: descriptor pool not created");
	}

	auto layoutIt = layouts.find(layout.id);
	if (layoutIt == layouts.end()) {
		throw std::runtime_error("VulkanResourceBinder: layout not found");
	}

	VkDescriptorSetLayout vkLayout = layoutIt->second;

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &vkLayout;

	VkDescriptorSet vkSet;
	if (vkAllocateDescriptorSets(device, &allocInfo, &vkSet) != VK_SUCCESS) {
		throw std::runtime_error("VulkanResourceBinder: failed to allocate descriptor set");
	}

	// Store set and return handle
	uint64_t id = nextSetId++;
	sets[id] = vkSet;

	ResourceSetHandle handle;
	handle.id = id;
	return handle;
}

void VulkanResourceBinder::freeSet(ResourceSetHandle set) {
	if (!set.isValid())
		return;

	auto it = sets.find(set.id);
	if (it != sets.end()) {
		if (pool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
			vkFreeDescriptorSets(device, pool, 1, &it->second);
		}
		sets.erase(it);
	}
}

void VulkanResourceBinder::updateBufferBindings(ResourceSetHandle set,
												const std::vector<ResourceBufferBinding>& bindings) {
	if (!set.isValid() || bindings.empty())
		return;

	auto setIt = sets.find(set.id);
	if (setIt == sets.end()) {
		std::cerr << "VulkanResourceBinder::updateBufferBindings: set not found" << std::endl;
		return;
	}

	std::vector<VkWriteDescriptorSet> writes;
	std::vector<VkDescriptorBufferInfo> bufferInfos;

	writes.reserve(bindings.size());
	bufferInfos.reserve(bindings.size());

	for (const auto& binding : bindings) {
		// Get VkBuffer from BufferHandle
		VkBuffer vkBuffer = VK_NULL_HANDLE;
		if (bufferManager) {
			// Cast to VulkanBuffer to get native handle
			auto* vb = static_cast<VulkanBuffer*>(bufferManager);
			vkBuffer = vb->getVulkanBuffer(binding.buffer);
		}

		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = vkBuffer;
		bufferInfo.offset = binding.offset;
		bufferInfo.range = binding.range == ~0ull ? VK_WHOLE_SIZE : binding.range;
		bufferInfos.push_back(bufferInfo);

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = setIt->second;
		write.dstBinding = binding.binding;
		write.dstArrayElement = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // TODO: Detect type from layout
		write.descriptorCount = 1;
		write.pBufferInfo = &bufferInfos.back();

		writes.push_back(write);
	}

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanResourceBinder::updateImageBindings(ResourceSetHandle set,
											   const std::vector<ResourceImageBinding>& bindings) {
	if (!set.isValid() || bindings.empty())
		return;

	auto setIt = sets.find(set.id);
	if (setIt == sets.end()) {
		std::cerr << "VulkanResourceBinder::updateImageBindings: set not found" << std::endl;
		return;
	}

	std::vector<VkWriteDescriptorSet> writes;
	std::vector<VkDescriptorImageInfo> imageInfos;

	writes.reserve(bindings.size());
	imageInfos.reserve(bindings.size());

	for (const auto& binding : bindings) {
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = static_cast<VkImageView>(binding.imageView);
		imageInfo.sampler = static_cast<VkSampler>(binding.sampler);
		imageInfos.push_back(imageInfo);

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = setIt->second;
		write.dstBinding = binding.binding;
		write.dstArrayElement = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = 1;
		write.pImageInfo = &imageInfos.back();

		writes.push_back(write);
	}

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanResourceBinder::updateSet(ResourceSetHandle set,
									 const std::vector<ResourceBufferBinding>& bufferBindings,
									 const std::vector<ResourceImageBinding>& imageBindings) {
	if (!set.isValid())
		return;
	if (bufferBindings.empty() && imageBindings.empty())
		return;

	auto setIt = sets.find(set.id);
	if (setIt == sets.end()) {
		std::cerr << "VulkanResourceBinder::updateSet: set not found" << std::endl;
		return;
	}

	std::vector<VkWriteDescriptorSet> writes;
	std::vector<VkDescriptorBufferInfo> bufferInfos;
	std::vector<VkDescriptorImageInfo> imageInfos;

	writes.reserve(bufferBindings.size() + imageBindings.size());
	bufferInfos.reserve(bufferBindings.size());
	imageInfos.reserve(imageBindings.size());

	// Add buffer bindings
	for (const auto& binding : bufferBindings) {
		VkBuffer vkBuffer = VK_NULL_HANDLE;
		if (bufferManager) {
			auto* vb = static_cast<VulkanBuffer*>(bufferManager);
			vkBuffer = vb->getVulkanBuffer(binding.buffer);
		}

		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = vkBuffer;
		bufferInfo.offset = binding.offset;
		bufferInfo.range = binding.range == ~0ull ? VK_WHOLE_SIZE : binding.range;
		bufferInfos.push_back(bufferInfo);

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = setIt->second;
		write.dstBinding = binding.binding;
		write.dstArrayElement = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; // TODO: Detect from layout
		write.descriptorCount = 1;
		write.pBufferInfo = &bufferInfos.back();

		writes.push_back(write);
	}

	// Add image bindings
	for (const auto& binding : imageBindings) {
		VkDescriptorImageInfo imageInfo{};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = static_cast<VkImageView>(binding.imageView);
		imageInfo.sampler = static_cast<VkSampler>(binding.sampler);
		imageInfos.push_back(imageInfo);

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = setIt->second;
		write.dstBinding = binding.binding;
		write.dstArrayElement = 0;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		write.descriptorCount = 1;
		write.pImageInfo = &imageInfos.back();

		writes.push_back(write);
	}

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void* VulkanResourceBinder::getNativeHandle(ResourceSetHandle set) const {
	if (!set.isValid())
		return nullptr;

	auto it = sets.find(set.id);
	if (it == sets.end())
		return nullptr;

	// Return pointer to VkDescriptorSet
	return const_cast<VkDescriptorSet*>(&it->second);
}

// ============================================================================
// Helper Functions
// ============================================================================

VkDescriptorType VulkanResourceBinder::toVulkanDescriptorType(ResourceType type) const {
	switch (type) {
	case ResourceType::UniformBuffer:
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	case ResourceType::UniformBufferDynamic:
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	case ResourceType::StorageBuffer:
		return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	case ResourceType::Sampler:
		return VK_DESCRIPTOR_TYPE_SAMPLER;
	case ResourceType::Texture:
		return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	case ResourceType::CombinedTextureSampler:
		return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	default:
		return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	}
}

VkShaderStageFlags VulkanResourceBinder::toVulkanShaderStages(ShaderStage stages) const {
	VkShaderStageFlags flags = 0;

	if (hasFlag(stages, ShaderStage::Vertex))
		flags |= VK_SHADER_STAGE_VERTEX_BIT;
	if (hasFlag(stages, ShaderStage::Fragment))
		flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
	if (hasFlag(stages, ShaderStage::Compute))
		flags |= VK_SHADER_STAGE_COMPUTE_BIT;
	if (hasFlag(stages, ShaderStage::Geometry))
		flags |= VK_SHADER_STAGE_GEOMETRY_BIT;

	return flags;
}

} // namespace Renderer
