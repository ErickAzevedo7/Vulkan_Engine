#pragma once

#include <cstdint>
#include <vector>

#include "components/LightComponent.h"
#include "renderer/RenderTypes.h"
#include "vulkan/vulkan_core.h"

// Forward declaration - use interface, not implementation
namespace Renderer {
class GraphicsBuffer;
class VulkanShadowMap;
} // namespace Renderer

class LightManager {
public:
	LightManager(Renderer::GraphicsBuffer* bufferMgr); // Accept interface
	~LightManager();

	// Update the uniform for a given frame index
	void init();
	void updateLight(uint32_t frame, const LightComponent::LightUniform& u);

	// Access to the underlying VkBuffer for descriptor writes
	VkBuffer getLightBuffer(uint32_t frame);
	Renderer::BufferHandle getLightBufferHandle(uint32_t frame) const;

	// Set buffer manager (for deferred initialization)
	void setBufferManager(Renderer::GraphicsBuffer* bufferMgr);

	void initDescriptorResources(VkDevice device, VkDescriptorPool pool);
	void createDescriptorSetLayout();
	void createDescriptorSets();
	void updateDescriptorSets(Renderer::VulkanShadowMap* shadowMap,
							  VkImageView irradianceMap,
							  VkSampler irradianceSampler,
							  VkImageView prefilterMap,
							  VkSampler prefilterSampler,
							  VkImageView brdfLut,
							  VkSampler brdfLutSampler);

	VkDescriptorSetLayout getDescriptorSetLayout() const {
		return descriptorSetLayout;
	}
	const std::vector<VkDescriptorSet>& getDescriptorSets() const {
		return descriptorSets;
	}

private:
	VkDevice device = VK_NULL_HANDLE;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> descriptorSets;

	Renderer::GraphicsBuffer* bufferManager; // Store interface pointer
	std::vector<Renderer::BufferHandle> lightBuffers;
	// Removed: std::vector<VkDeviceMemory> lightBufferMem;
};
