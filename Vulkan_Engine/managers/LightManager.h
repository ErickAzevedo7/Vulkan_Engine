#pragma once

#include <cstdint>
#include <vector>

#include "components/LightComponent.h"
#include "vulkan/vulkan_core.h"

class LightManager {
public:
	LightManager();
	~LightManager();

	// Update the uniform for a given frame index
	void updateLight(uint32_t frame, const LightComponent::LightUniform& u);

	// Access to the underlying VkBuffer for descriptor writes
	VkBuffer getLightBuffer(uint32_t frame);

private:
	std::vector<VkBuffer> lightBuffers;
	std::vector<VkDeviceMemory> lightBufferMem;
};
