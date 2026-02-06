#include "LightManager.h"

// Simple single-light manager: creates a uniform buffer per-frame and exposes
// it so descriptor sets can bind it at binding 2.

static std::vector<VkBuffer> lightBuffers;
static std::vector<VkDeviceMemory> lightBufferMem;

void LightManager::init() {
	uint32_t count = MAX_FRAMES_IN_FLIGHT;
	lightBuffers.resize(count);
	lightBufferMem.resize(count);

	// Allocate buffer for: colorIntensity(vec4) + direction(vec4) + positionType(vec4) + ambient(vec4) + diffuse(vec4) + specular(vec4) + attenuation(3 floats) + cutOff(float) + outerCutOff(float) = 6 vec4s + 5 floats (THIS IS TEMPORARY)
	VkDeviceSize bufferSize = sizeof(glm::vec4) * 6 + sizeof(float) * 5;
	for (uint32_t i = 0; i < count; ++i) {
		Utils::createBuffer(bufferSize,
		                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		                    lightBuffers[i], lightBufferMem[i]);
	}
}

void LightManager::cleanup() {
	for (size_t i = 0; i < lightBuffers.size(); ++i) {
		vkDestroyBuffer(VulkanCore::getDevice(), lightBuffers[i], nullptr);
		vkFreeMemory(VulkanCore::getDevice(), lightBufferMem[i], nullptr);
	}
	lightBuffers.clear();
	lightBufferMem.clear();
}

void LightManager::updateLight(uint32_t frame, const LightComponent::LightUniform& u) {
	if (frame >= lightBuffers.size()) return;
	void* data;
	
	// Write 6 vec4s: [0]=colorIntensity, [1]=direction, [2]=positionType, [3]=ambient, [4]=diffuse, [5]=specular (THIS IS TEMPORARY)
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

	VkDeviceSize bufferSize = sizeof(glm::vec4) * 6 + sizeof(float) * 5;
	vkMapMemory(VulkanCore::getDevice(), lightBufferMem[frame], 0, bufferSize, 0, &data);
	
	memcpy(data, &v0, sizeof(glm::vec4));
	memcpy(static_cast<char*>(data) + sizeof(glm::vec4) * 1, &v1, sizeof(glm::vec4));
	memcpy(static_cast<char*>(data) + sizeof(glm::vec4) * 2, &v2, sizeof(glm::vec4));
	memcpy(static_cast<char*>(data) + sizeof(glm::vec4) * 3, &v3, sizeof(glm::vec4));
	memcpy(static_cast<char*>(data) + sizeof(glm::vec4) * 4, &v4, sizeof(glm::vec4));
	memcpy(static_cast<char*>(data) + sizeof(glm::vec4) * 5, &v5, sizeof(glm::vec4));
	
	// Write attenuation values after the padded vec3s
	memcpy(reinterpret_cast<char*>(data) + sizeof(glm::vec4) * 6, &attKc, sizeof(float));
	memcpy(reinterpret_cast<char*>(data) + sizeof(glm::vec4) * 6 + sizeof(float), &attKl, sizeof(float));
	memcpy(reinterpret_cast<char*>(data) + sizeof(glm::vec4) * 6 + sizeof(float) * 2, &attKq, sizeof(float));
	
	// Write spotlight cutoff angles
	memcpy(reinterpret_cast<char*>(data) + sizeof(glm::vec4) * 6 + sizeof(float) * 3, &cutOff, sizeof(float));
	memcpy(reinterpret_cast<char*>(data) + sizeof(glm::vec4) * 6 + sizeof(float) * 4, &outerCutOff, sizeof(float));
	
	vkUnmapMemory(VulkanCore::getDevice(), lightBufferMem[frame]);
}

VkBuffer LightManager::getLightBuffer(uint32_t frame) {
	return lightBuffers.at(frame);
}
