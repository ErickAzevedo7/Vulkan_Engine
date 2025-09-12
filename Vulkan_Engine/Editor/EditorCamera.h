#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <vulkan/vulkan.h>
#include "vulkancore.h"

class EditorCamera
{
public:
	void init(VulkanCore* core) {
		engineCore = core;
	}

	void updateUniformBuffer(uint32_t currentImage);
private:
	VulkanCore* engineCore;
};

