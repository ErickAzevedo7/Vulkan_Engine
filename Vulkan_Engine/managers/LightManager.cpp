#include "LightManager.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "components/LightComponent.h"
#include "core/vulkancore.h"
#include "renderer/GraphicsBuffer.h" // Use interface, not VulkanBuffer
#include "renderer/RenderTypes.h"
#include "renderer/vulkan/VulkanBuffer.h" // Only for getVulkanBuffer()
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
