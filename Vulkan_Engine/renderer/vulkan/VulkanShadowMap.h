#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan_core.h>

#include "renderer/ShadowMap.h"

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float4.hpp"

namespace Renderer {

class VulkanDevice;

struct ShadowUBO {
	alignas(16) glm::mat4 lightSpaceMatrix[6];
	alignas(16) glm::vec4 lightPos_farPlane;
};

class VulkanShadowMap : public ShadowMap {
public:
	VulkanShadowMap() = default;
	~VulkanShadowMap() override;

	void init(void* device, uint32_t width, uint32_t height) override;
	void cleanup() override;

	void* getRenderPass() const override {
		return renderPass;
	}
	void* getFramebuffer() const override {
		return framebuffer;
	}
	void* getDepthImageView() const override {
		return depthImageView;
	}
	void* getDepth2DView() const override {
		return depth2DView;
	}
	void* getDepthCubeImageView() const override {
		return depthCubeImageView;
	}
	void* getDepthSampler() const override {
		return depthSampler;
	}

	uint32_t getWidth() const override {
		return width;
	}
	uint32_t getHeight() const override {
		return height;
	}

	VkPipeline getPipeline() const {
		return pipeline;
	}
	VkPipelineLayout getPipelineLayout() const;
	VkCommandBuffer getCommandBuffer(uint32_t currentFrame) const {
		return shadowCommandBuffers[currentFrame];
	}

	void recordShadowCommandBuffer(uint32_t currentFrame,
								   const glm::mat4* lightSpaceMatrices,
								   const glm::vec4& lightPos_farPlane);

	// Uploads per-frame light data into the shadow UBO buffer.
	void updateShadowUBO(uint32_t frame, const glm::mat4* lightSpaceMatrices, const glm::vec4& lightPos_farPlane);

	// Implement the interface for generating light projection matrices
	void calculateShadowMatrices(const glm::vec4& positionType,
								 const glm::vec4& direction,
								 float far_plane,
								 float outerCutOff,
								 glm::mat4 outMatrices[6],
								 glm::vec4& outLightPosFarPlane) override;

	// Returns the descriptor set for this frame (contains the shadow UBO).
	VkDescriptorSet getDescriptorSet(uint32_t frame) const;

private:
	void createRenderPass();
	void createDepthResources();
	void createFramebuffer();
	void createSampler();
	void createShadowDescriptorSetLayout();
	void createShadowPipelineLayout();
	void createShadowUBOs();
	void createShadowDescriptorPool();
	void createShadowDescriptorSets();
	void createPipeline();
	VkShaderModule createShaderModule(const std::vector<char>& code);
	VkFormat findDepthFormat();

	VulkanDevice* vulkanDevice = nullptr;
	uint32_t width = 0;
	uint32_t height = 0;

	VkImage depthImage = VK_NULL_HANDLE;
	VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
	VkImageView depthImageView = VK_NULL_HANDLE; // Used for framebuffer (2D Array)
	VkImageView depth2DView = VK_NULL_HANDLE;
	VkImageView depthCubeImageView = VK_NULL_HANDLE; // Used for samplerCube
	VkSampler depthSampler = VK_NULL_HANDLE;
	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkFramebuffer framebuffer = VK_NULL_HANDLE;
	VkPipeline pipeline = VK_NULL_HANDLE;

	// Shadow-specific descriptor / pipeline resources
	VkDescriptorSetLayout shadowDescriptorSetLayout = VK_NULL_HANDLE;
	VkPipelineLayout shadowPipelineLayout = VK_NULL_HANDLE;
	VkDescriptorPool shadowDescriptorPool = VK_NULL_HANDLE;
	std::vector<VkDescriptorSet> shadowDescriptorSets;
	std::vector<VkCommandBuffer> shadowCommandBuffers;

	// Per-frame shadow UBO buffers (uploaded every frame)
	std::vector<VkBuffer> shadowUBOBuffers;
	std::vector<VkDeviceMemory> shadowUBOMemory;
	std::vector<void*> shadowUBOMapped;
};

} // namespace Renderer
