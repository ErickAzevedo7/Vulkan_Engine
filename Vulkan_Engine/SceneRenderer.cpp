#include "SceneRenderer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

#include "components/MeshComponent.h"
#include "components/Transform.h"
#include "context/ResourceContext.h"
#include "core/utils/Utils.h"
#include "core/vulkancore.h"
#include "Entity.h"
#include "managers/LightManager.h"
#include "managers/MeshManager.h"
#include "managers/SceneManager.h"
#include "renderer/RenderCommandList.h"
#include "renderer/vulkan/VulkanCommandList.h"
#include "Scene.h"
#include "vulkan/vulkan_core.h"

#include "glm/ext/matrix_float4x4.hpp"

// Initialize static members
ResourceContext* SceneRenderer::resources = nullptr;
Scene* SceneRenderer::sceneOverride = nullptr;

VkDevice SceneRenderer::device = VK_NULL_HANDLE;
VkDescriptorPool SceneRenderer::descriptorPool = VK_NULL_HANDLE;
VkDescriptorSetLayout SceneRenderer::perObjectDescriptorSetLayout = VK_NULL_HANDLE;
std::vector<VkDescriptorSet> SceneRenderer::perObjectDescriptorSets;
std::vector<VkBuffer> SceneRenderer::uniformBuffers;
std::vector<VkDeviceMemory> SceneRenderer::uniformBuffersMemory;
std::vector<void*> SceneRenderer::uniformBuffersMapped;
VkDeviceSize SceneRenderer::dynamicAlignment = 0;

void SceneRenderer::init(ResourceContext* resContext) {
	SceneRenderer::resources = resContext;
}

void SceneRenderer::setSceneOverride(Scene* overrideScene) {
	sceneOverride = overrideScene;
}

Scene* SceneRenderer::resolveScene() {
	if (sceneOverride) {
		return sceneOverride;
	}
	if (!resources) {
		return nullptr;
	}
	return resources->getSceneManager().getActiveScene();
}

Scene* SceneRenderer::getSceneForRendering() {
	return resolveScene();
}

void SceneRenderer::initDescriptorResources(VkDevice dev, VkDescriptorPool pool, VkPhysicalDevice physicalDevice) {
	device = dev;
	descriptorPool = pool;
	createPerObjectDescriptorSetLayout();
	createUniformBuffers(physicalDevice);
	createPerObjectDescriptorSets();
}

void SceneRenderer::cleanup() {
	if (device == VK_NULL_HANDLE)
		return;

	for (size_t i = 0; i < uniformBuffers.size(); i++) {
		vkDestroyBuffer(device, uniformBuffers[i], nullptr);
		vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
	}
	uniformBuffers.clear();
	uniformBuffersMemory.clear();
	uniformBuffersMapped.clear();

	if (perObjectDescriptorSetLayout != VK_NULL_HANDLE) {
		vkDestroyDescriptorSetLayout(device, perObjectDescriptorSetLayout, nullptr);
		perObjectDescriptorSetLayout = VK_NULL_HANDLE;
	}

	device = VK_NULL_HANDLE;
	sceneOverride = nullptr;
}

void SceneRenderer::createPerObjectDescriptorSetLayout() {
	if (device == VK_NULL_HANDLE)
		return;

	VkDescriptorSetLayoutBinding perObjectLayoutBinding{};
	perObjectLayoutBinding.binding = 0;
	perObjectLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	perObjectLayoutBinding.descriptorCount = 1;
	perObjectLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	perObjectLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &perObjectLayoutBinding;

	if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &perObjectDescriptorSetLayout) != VK_SUCCESS) {
		throw std::runtime_error("fails to create per-object descriptor set layout!");
	}
}

void SceneRenderer::createUniformBuffers(VkPhysicalDevice physicalDevice) {
	VkPhysicalDeviceProperties properties{};
	vkGetPhysicalDeviceProperties(physicalDevice, &properties);

	size_t minUboAlignment = properties.limits.minUniformBufferOffsetAlignment;
	dynamicAlignment = sizeof(PerObjectUBO);
	if (minUboAlignment > 0) {
		dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}

	size_t bufferSize = kMaxPerObjectUbos * dynamicAlignment;
	uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
	uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		Utils::createBuffer(bufferSize,
							VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
							VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
							uniformBuffers[i],
							uniformBuffersMemory[i]);

		vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
	}
}

void SceneRenderer::createPerObjectDescriptorSets() {
	perObjectDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

	std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, perObjectDescriptorSetLayout);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = descriptorPool;
	allocInfo.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT);
	allocInfo.pSetLayouts = layouts.data();

	if (vkAllocateDescriptorSets(device, &allocInfo, perObjectDescriptorSets.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate per-object descriptor sets!");
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = uniformBuffers[i];
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(PerObjectUBO);

		VkWriteDescriptorSet descriptorWrite{};
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = perObjectDescriptorSets[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
	}
}

void SceneRenderer::renderScene(Renderer::RenderCommandList& commandList,
								VkPipeline pipeline,
								VkPipelineLayout pipelineLayout,
								uint32_t currentFrame,
								VkDescriptorSet globalSet) {
	Scene* scene = resolveScene();
	if (!scene)
		return;

	// Bind GlobalUBO (set=0) and Lighting/Environment (set=1) once at the top of every scene pass.
	// We retrieve the lighting descriptor set for the current frame
	VkDescriptorSet lightingSet = resources->getLightManager().getDescriptorSets()[currentFrame];
	std::array<VkDescriptorSet, 2> globalSets = {globalSet, lightingSet};

	if (auto* vkList = dynamic_cast<Renderer::VulkanCommandList*>(&commandList)) {
		vkCmdBindDescriptorSets(vkList->getCommandBuffer(),
								VK_PIPELINE_BIND_POINT_GRAPHICS,
								pipelineLayout,
								0 /*firstSet*/,
								2 /*descriptorSetCount*/,
								globalSets.data(),
								0,
								nullptr);
	}

	auto* entities = scene->getEntities();
	if (!entities)
		return;

	for (size_t perObjectIndex = 0; perObjectIndex < entities->size(); ++perObjectIndex) {
		Entity* entity = (*entities)[perObjectIndex].get();
		if (!entity)
			continue;

		renderEntity(entity,
					 commandList,
					 pipeline,
					 pipelineLayout,
					 currentFrame,
					 0,
					 static_cast<uint32_t>(perObjectIndex));
	}
}

void SceneRenderer::renderEntity(const Entity* entity,
								 Renderer::RenderCommandList& commandList,
								 VkPipeline pipeline,
								 VkPipelineLayout pipelineLayout,
								 uint32_t currentFrame,
								 int useMousePick,
								 uint32_t perObjectIndex) {
	const MeshComponent* meshComp = entity->getComponent<MeshComponent>();

	if (!meshComp) {
		return;
	}

	meshComp->render(commandList,
					 pipeline,
					 pipelineLayout,
					 currentFrame,
					 useMousePick,
					 perObjectIndex,
					 resources->getMeshManager(),
					 resources->getResourceBinder());
}

void SceneRenderer::renderOutlineSelected(Renderer::RenderCommandList& commandList,
										  VkPipeline outlinePipeline,
										  VkPipelineLayout outlinePipelineLayout,
										  VkDescriptorSet outlineDescriptorSet) {
	Scene* scene = resolveScene();
	if (!scene)
		return;
	size_t entities = scene->getEntityCount();
	for (int i = 1; i <= entities; ++i) {
		Entity* entity = &scene->getEntity(i);
		if (entity->isSelected) {
			int selectedID = entity->getID();
			commandList.pushConstants(outlinePipelineLayout,
									  16, // VK_SHADER_STAGE_FRAGMENT_BIT mapping
									  0,
									  sizeof(int),
									  &selectedID);

			commandList.bindPipeline(outlinePipeline);
			void* descSets[] = {outlineDescriptorSet};
			commandList.bindDescriptorSets(outlinePipelineLayout, descSets, 1, nullptr, 0);
			commandList.draw(3, 1, 0, 0); // Full-screen triangle
		}
	}
}

void SceneRenderer::renderMousePick(Renderer::RenderCommandList& commandList,
									VkPipeline pipeline,
									VkPipelineLayout pipelineLayout,
									uint32_t currentFrame,
									VkDescriptorSet globalSet) {
	Scene* scene = resolveScene();
	if (!scene)
		return;

	// Bind GlobalUBO (set=0) and Lighting/Environment (set=1)
	VkDescriptorSet lightingSet = resources->getLightManager().getDescriptorSets()[currentFrame];
	std::array<VkDescriptorSet, 2> globalSets = {globalSet, lightingSet};

	if (auto* vkList = dynamic_cast<Renderer::VulkanCommandList*>(&commandList)) {
		vkCmdBindDescriptorSets(vkList->getCommandBuffer(),
								VK_PIPELINE_BIND_POINT_GRAPHICS,
								pipelineLayout,
								0 /*firstSet*/,
								2,
								globalSets.data(),
								0,
								nullptr);
	}

	auto* entities = scene->getEntities();
	if (!entities)
		return;

	for (size_t perObjectIndex = 0; perObjectIndex < entities->size(); ++perObjectIndex) {
		Entity* entity = (*entities)[perObjectIndex].get();
		if (!entity)
			continue;

		renderEntity(entity,
					 commandList,
					 pipeline,
					 pipelineLayout,
					 currentFrame,
					 1,
					 static_cast<uint32_t>(perObjectIndex));
	}
}

void SceneRenderer::renderShadows(Renderer::RenderCommandList& commandList,
								  VkPipeline shadowPipeline,
								  VkPipelineLayout shadowPipelineLayout,
								  VkDescriptorSet shadowDescriptorSet) {
	Scene* scene = resolveScene();
	if (!scene)
		return;

	// Bind shadow pipeline and its descriptor set before drawing anything
	commandList.bindPipeline(shadowPipeline);

	void* pDescriptorSets[] = {shadowDescriptorSet};
	commandList.bindDescriptorSets(shadowPipelineLayout, pDescriptorSets, 1, nullptr, 0);

	auto entities = scene->getEntities();
	MeshManager& meshManager = resources->getMeshManager();

	for (const auto& entPtr : *entities) {
		Entity* e = entPtr.get();
		if (!e)
			continue;
		auto* meshComp = e->getComponent<MeshComponent>();
		auto* transform = e->getComponent<Transform>();
		if (!meshComp || !transform)
			continue;

		Mesh* mesh = meshComp->GetMesh();
		if (!mesh)
			continue;

		// Push the model matrix (64 bytes) for this object
		glm::mat4 model = transform->getMatrix();
		commandList.pushConstants(shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &model);

		// Bind Geometry Buffers
		void* pBuffers[] = {meshManager.getVertexBuffer(*mesh)};
		size_t offsets[] = {0};
		commandList.bindVertexBuffers(pBuffers, offsets, 1);
		commandList.bindIndexBuffer(meshManager.getIndexBuffer(*mesh));

		commandList.drawIndexed(mesh->indexCount, 1, 0, 0, 0);
	}
}
