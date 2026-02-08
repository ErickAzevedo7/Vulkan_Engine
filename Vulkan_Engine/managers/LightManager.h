#pragma once

#include "components/LightComponent.h"
#include "vulkan/vulkan_core.h"

#include <cstdint>

class LightManager {
public:
	static void init();
	static void cleanup();

	// Update the uniform for a given frame index
	static void updateLight(uint32_t frame, const LightComponent::LightUniform& u);

	// Access to the underlying VkBuffer for descriptor writes
	static VkBuffer getLightBuffer(uint32_t frame);
};
