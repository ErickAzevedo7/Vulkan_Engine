#pragma once

#include <cstdint>
#include <vector>

#include "components/LightComponent.h"
#include "renderer/RenderTypes.h"
#include "vulkan/vulkan_core.h"

// Forward declaration - use interface, not implementation
namespace Renderer {
class GraphicsBuffer;
}

class LightManager {
public:
	LightManager(Renderer::GraphicsBuffer* bufferMgr); // Accept interface
	~LightManager();

	// Update the uniform for a given frame index
	void init();
	void updateLight(uint32_t frame, const LightComponent::LightUniform& u);

	// Access to the underlying VkBuffer for descriptor writes
	VkBuffer getLightBuffer(uint32_t frame);

	// Set buffer manager (for deferred initialization)
	void setBufferManager(Renderer::GraphicsBuffer* bufferMgr);

private:
	Renderer::GraphicsBuffer* bufferManager; // Store interface pointer
	std::vector<Renderer::BufferHandle> lightBuffers;
	// Removed: std::vector<VkDeviceMemory> lightBufferMem;
};
