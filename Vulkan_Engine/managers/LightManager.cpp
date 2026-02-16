#include "LightManager.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "components/LightComponent.h"
#include "core/vulkancore.h"
#include "renderer/GraphicsBuffer.h" // Use interface, not VulkanBuffer
#include "renderer/vulkan/VulkanBuffer.h" // Only for getVulkanBuffer()
#include "vulkan/vulkan_core.h"

#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/geometric.hpp"
#include "glm/trigonometric.hpp"

// light manager: creates a uniform buffer per-frame and exposes
// it so descriptor sets can bind it at binding 2.

LightManager::LightManager(Renderer::GraphicsBuffer* bufferMgr) : bufferManager(bufferMgr) {
	// Constructor - store buffer manager reference (interface pointer)
}

void LightManager::init() {
	// Initialize all resources
	uint32_t count = MAX_FRAMES_IN_FLIGHT;
	lightBuffers.resize(count);

	// Calculate buffer size: 6 vec4s + 6 floats
	// colorIntensity(vec4) + direction(vec4) + positionType(vec4) + ambient(vec4)
	// + diffuse(vec4) + specular(vec4) + attenuation(3 floats) + cutOff(float)
	// + outerCutOff(float) + useBlinnPhong(int)
	size_t bufferSize = sizeof(glm::vec4) * 6 + sizeof(float) * 6;

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

	// Prepare data to upload
	// Write 6 vec4s: [0]=colorIntensity, [1]=direction, [2]=positionType, [3]=ambient, [4]=diffuse, [5]=specular
	glm::vec4 v0 = glm::vec4(u.color, u.intensity);
	glm::vec3 dir = glm::normalize(u.direction);
	glm::vec4 v1 = glm::vec4(dir, 0.0f);
	glm::vec4 v2 = glm::vec4(u.position, static_cast<float>(u.type));
	glm::vec4 v3 = glm::vec4(u.ambient, 0.0f);
	glm::vec4 v4 = glm::vec4(u.diffuse, 0.0f);
	glm::vec4 v5 = glm::vec4(u.specular, 0.0f);

	// Attenuation values
	float attKc = u.attenuationKc;
	float attKl = u.attenuationKl;
	float attKq = u.attenuationKq;

	// Spotlight cutoff angles (stored as cosine for efficient comparison in shader)
	float cutOff = glm::cos(u.innerCone);
	float outerCutOff = glm::cos(u.outerCone);
	int blinnPhongFlag = u.useBlinnPhong;

	// Build buffer data
	size_t bufferSize = sizeof(glm::vec4) * 6 + sizeof(float) * 6;
	char tempBuffer[256]; // Stack allocation for small uniform

	memcpy(tempBuffer, &v0, sizeof(glm::vec4));
	memcpy(tempBuffer + sizeof(glm::vec4) * 1, &v1, sizeof(glm::vec4));
	memcpy(tempBuffer + sizeof(glm::vec4) * 2, &v2, sizeof(glm::vec4));
	memcpy(tempBuffer + sizeof(glm::vec4) * 3, &v3, sizeof(glm::vec4));
	memcpy(tempBuffer + sizeof(glm::vec4) * 4, &v4, sizeof(glm::vec4));
	memcpy(tempBuffer + sizeof(glm::vec4) * 5, &v5, sizeof(glm::vec4));

	// Write attenuation values after the vec4s
	memcpy(tempBuffer + sizeof(glm::vec4) * 6, &attKc, sizeof(float));
	memcpy(tempBuffer + sizeof(glm::vec4) * 6 + sizeof(float), &attKl, sizeof(float));
	memcpy(tempBuffer + sizeof(glm::vec4) * 6 + sizeof(float) * 2, &attKq, sizeof(float));

	// Write spotlight cutoff angles
	memcpy(tempBuffer + sizeof(glm::vec4) * 6 + sizeof(float) * 3, &cutOff, sizeof(float));
	memcpy(tempBuffer + sizeof(glm::vec4) * 6 + sizeof(float) * 4, &outerCutOff, sizeof(float));

	// Write useBlinnPhong flag
	memcpy(tempBuffer + sizeof(glm::vec4) * 6 + sizeof(float) * 5, &blinnPhongFlag, sizeof(int));

	// Update buffer using abstraction
	bufferManager->updateBuffer(lightBuffers[frame], tempBuffer, bufferSize);
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
