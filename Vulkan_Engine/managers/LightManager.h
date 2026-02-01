#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include "core/vulkancore.h"
#include "components/LightComponent.h"
#include <stdexcept>
#include <array>

class LightManager {
public:
  static void init();
  static void cleanup();

  // Update the uniform for a given frame index
  static void updateLight(uint32_t frame, const LightComponent::LightUniform& u);

  // Access to the underlying VkBuffer for descriptor writes
  static VkBuffer getLightBuffer(uint32_t frame);
};
