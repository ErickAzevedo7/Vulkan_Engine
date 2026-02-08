#include <array>
#include <corecrt_terminate.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <stdio.h>
#include <vector>

#include "GLFW/glfw3.h"
#include "Scene.h"
#include "vulkan/vulkan_core.h"

#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#define fontPath "C:\\Windows\\Fonts\\segoeuisl.ttf" // Windows UI font
#define boldFontPath "C:\\Windows\\Fonts\\segoeuib.ttf" // Windows UI bold font

#include <iostream>

#include "components/LightComponent.h"
#include "components/MeshComponent.h"
#include "context/ResourceContext.h"
#include "core/vulkancore.h"
#include "Editor/EditorCamera.h"
#include "Editor/MousePick.h"
#include "Editor/ViewPort.h"
#include "factory/EntityFactory.h"
#include "gizmos/ImGuizmo.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "managers/LightManager.h"
#include "managers/MaterialManager.h"
#include "managers/SceneManager.h"
#include "postprocess/outline.h"
#include "SceneRenderer.h"
#include "ui/AssetBrowser.h"
#include "ui/InspectorUi.h"
#include "ui/SceneUi.h"
#include "ui/UIManager.h"

// GLOBAL VARIABLES
double deltaTime = 0.0; // Time between current frame and last frame
double lastFrame = 0.0; // Time of last frame

class VulkanEngine {
public:
	void run() {
		// Initialize Vulkan
		engineCore.initWindow();
		engineCore.initVulkan();

		// Initialize resource managers (textures, materials, lights)
		ResourceContext::init();
		ResourceContext::loadDefaults();

		SceneRenderer::init(&engineCore);
		mousePick.init(&engineCore);
		viewPort.init(&engineCore, mousePick.getMousePickExtent());
		outline.init(&engineCore,
					 mousePick.getMousePickImageViews(),
					 viewPort.m_ViewportImageViews,
					 mousePick.getMousePickExtent());
		editorCamera.init(&engineCore);
		SceneManager::loadDefaults();

		// Create a hardcoded test light entity so lighting can be verified (TEMPORARY)
		{
			Scene* scene = SceneManager::getActiveScene();
			if (scene) {
				Entity& lightEntity = EntityFactory::createLight(scene,
																 "TestDirectionalLight",
																 LightType::Directional,
																 glm::vec3(0.0f, 10.0f, 0.0f),
																 glm::vec3(1.0f, 0.95f, 0.9f),
																 10.0f);

				// Set the direction for the directional light
				LightComponent* lc = lightEntity.getComponent<LightComponent>();
				if (lc) {
					lc->direction = glm::vec3(0.0f, -1.0f, 0.0f);
				}
			}
		}
		init();

		changeImGuizmoStyle();
		mainLoop();

		outline.cleanup();
		viewPort.cleanup();
		mousePick.cleanup();
		ResourceContext::cleanup();

		cleanup();
		engineCore.cleanup();
	}

	void drawFrame() {
		int width = 0, height = 0;
		glfwGetFramebufferSize(engineCore.getWindow(), &width, &height);
		while (width == 0 || height == 0) {
			glfwWaitEvents();
			return;
		}

		vkWaitForFences(VulkanCore::getDevice(),
						1,
						&engineCore.getInFlightFences()[VulkanCore::getCurrentFrame()],
						VK_TRUE,
						UINT64_MAX);
		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(VulkanCore::getDevice(),
												engineCore.getSwapChain(),
												UINT64_MAX,
												engineCore.getImageAvailableSemaphores()[VulkanCore::getCurrentFrame()],
												VK_NULL_HANDLE,
												&imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			engineCore.recreateSwapChain();
			return;
		} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		vkResetFences(VulkanCore::getDevice(), 1, &engineCore.getInFlightFences()[VulkanCore::getCurrentFrame()]);

		vkResetCommandBuffer(engineCore.getCommandBuffers()[VulkanCore::getCurrentFrame()], 0);

		// (TEMPORARY)
		// Update per-frame camera UBO first so command buffer recordings use up-to-date data
		editorCamera.updateUniformBuffer(VulkanCore::getCurrentFrame());

		// Update simple single light (for now static directional light)
		{
			uint32_t frame = VulkanCore::getCurrentFrame();
			// Find the first LightComponent in the active scene. If none found,
			// upload a zero-intensity light so the scene is dark.
			LightComponent::LightUniform lu{};
			bool foundLight = false;
			Scene* scene = SceneManager::getActiveScene();
			if (scene) {
				auto entities = scene->getEntities();
				for (const auto& entPtr : *entities) {
					Entity* e = entPtr.get();
					if (!e)
						continue;
					if (auto lc = e->getComponent<LightComponent>()) {
						lu = lc->getLightUniform();
						foundLight = true;
						break;
					}
				}
			}
			if (!foundLight) {
				// no lights in scene: make sure shader receives zero intensity
				lu.color = glm::vec3(0.0f);
				lu.intensity = 0.0f;
			}
			ResourceContext::getLightManager().updateLight(frame, lu);

			// Update all material descriptor sets for this frame to point binding 2 to the light buffer
			const auto& mats = ResourceContext::getMaterialManager().getAllMaterials();
			for (const auto& pair : mats) {
				Material* mat = pair.second;
				if (!mat)
					continue;
				if (mat->descriptorSets.size() <= frame)
					continue;
				VkDescriptorBufferInfo lightInfo{};
				lightInfo.buffer = ResourceContext::getLightManager().getLightBuffer(frame);
				lightInfo.offset = 0;
				// LightManager now allocates: 6 vec4s + 5 floats (3 attenuation + 2 cutoff angles)
				lightInfo.range = sizeof(glm::vec4) * 6 + sizeof(float) * 5;

				VkWriteDescriptorSet write{};
				write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				write.dstSet = mat->descriptorSets[frame];
				write.dstBinding = 2;
				write.dstArrayElement = 0;
				write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
				write.descriptorCount = 1;
				write.pBufferInfo = &lightInfo;
				vkUpdateDescriptorSets(VulkanCore::getDevice(), 1, &write, 0, nullptr);
			}
		}

		mousePick.recordMousePickCommandBuffer(mousePick.mousePickCommandBuffers[VulkanCore::getCurrentFrame()],
											   imageIndex);

		viewPort.recordViewportCommandBuffer(viewPort.m_ViewportCommandBuffers[VulkanCore::getCurrentFrame()],
											 imageIndex);

		outline.recordOutlineCommandBuffer(outline.outlineCommandBuffers[VulkanCore::getCurrentFrame()], imageIndex);

		recordImguiCommandBuffer(uiManager.getCommandBuffers()[VulkanCore::getCurrentFrame()], imageIndex);

		std::array<VkCommandBuffer, 4> submitCommandBuffers = {
			mousePick.mousePickCommandBuffers[VulkanCore::getCurrentFrame()],
			viewPort.m_ViewportCommandBuffers[VulkanCore::getCurrentFrame()],
			outline.outlineCommandBuffers[VulkanCore::getCurrentFrame()],
			uiManager.getCommandBuffers()[VulkanCore::getCurrentFrame()],
		};

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = {engineCore.getImageAvailableSemaphores()[VulkanCore::getCurrentFrame()]};
		VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;

		submitInfo.commandBufferCount = static_cast<uint32_t>(submitCommandBuffers.size());
		submitInfo.pCommandBuffers = submitCommandBuffers.data();

		VkSemaphore signalSemaphores[] = {engineCore.getRenderFinishedSemaphores()[VulkanCore::getCurrentFrame()]};
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		if (vkQueueSubmit(VulkanCore::getGraphicsQueue(),
						  1,
						  &submitInfo,
						  engineCore.getInFlightFences()[VulkanCore::getCurrentFrame()]) != VK_SUCCESS) {
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;

		VkSwapchainKHR swapChains[] = {engineCore.getSwapChain()};
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr; // Optional

		result = vkQueuePresentKHR(VulkanCore::getPresentQueue(), &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || engineCore.getFramebufferResized()) {
			engineCore.setFramebufferResized(false);
			engineCore.recreateSwapChain();
		} else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}

		engineCore.setCurrentFrame((VulkanCore::getCurrentFrame() + 1) % MAX_FRAMES_IN_FLIGHT);
	}

	void mainLoop() {
		sceneTexture.resize(viewPort.m_ViewportImageViews.size());
		for (uint32_t i = 0; i < viewPort.m_ViewportImageViews.size(); i++)
			sceneTexture[i] = ImGui_ImplVulkan_AddTexture(engineCore.getTextureSampler(),
														  outline.outlineColorImageViews[i],
														  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		while (!glfwWindowShouldClose(engineCore.getWindow())) {
			double currentFrame = glfwGetTime();
			deltaTime = currentFrame - lastFrame;
			lastFrame = currentFrame;
			glfwPollEvents();

			if (engineCore.getSwapChainRecreated()) {
				recreateRenderPasses();

				engineCore.setSwapChainRecreated(false);
			}

			uiManager.beginFrame();
			ImGuizmo::BeginFrame();
			ImGuizmo::Enable(true);

			ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::DockSpaceOverViewport(0, nullptr, dockspaceFlags);
			ImGui::PopStyleVar();

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

			ImGui::Begin("Viewport");

			ImVec2 viewportSize = ImGui::GetContentRegionAvail();

			uint32_t viewportWidth = (viewportSize.x > 0.0f) ? static_cast<uint32_t>(viewportSize.x) : 0;
			uint32_t viewportHeight = (viewportSize.y > 0.0f) ? static_cast<uint32_t>(viewportSize.y) : 0;

			this->viewportExtent = VkExtent2D{viewportWidth, viewportHeight};

			if (viewportExtent.width > 0 && viewportExtent.height > 0 &&
				(viewportExtent.width != mousePick.mousePickExtent.width ||
				 viewportExtent.height != mousePick.mousePickExtent.height)) {
				mousePick.mousePickExtent = viewportExtent;
				EditorCamera::setExtent(viewportExtent);

				recreateRenderPasses();
			}

			inputProcess();

			ImDrawList* draw_list = ImGui::GetWindowDrawList();

			// Get the current cursor screen position as a reference point
			ImVec2 p = ImGui::GetCursorScreenPos();

			ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

			ImGui::Image((ImTextureID)sceneTexture[VulkanCore::getCurrentFrame()],
						 ImVec2{viewportPanelSize.x, viewportPanelSize.y});

			ImGui::SetCursorScreenPos(p);

			EditorCamera::drawGuizmo();

			ImGui::End();
			ImGui::PopStyleVar(2);

			SceneUi::render();

			InspectorUi::render();

			AssetBrowser::render();

			if (ImGui::BeginMainMenuBar()) {
				if (ImGui::BeginMenu("Entities")) {
					if (ImGui::MenuItem("Create Empty Entity")) {
						EntityFactory::createEmpty(SceneManager::getActiveScene(), "Empty Entity");
					}

					if (ImGui::MenuItem("Create Cube")) {
						EntityFactory::createPrimitive(SceneManager::getActiveScene(), "Cube", "cube");
					}
					if (ImGui::MenuItem("Create Sphere")) {
						EntityFactory::createPrimitive(SceneManager::getActiveScene(), "Sphere", "sphere");
					}
					if (ImGui::MenuItem("Create Quad")) {
						EntityFactory::createPrimitive(SceneManager::getActiveScene(), "Quad", "quad");
					}
					ImGui::EndMenu();
				}
				ImGui::EndMainMenuBar();
			}

			ImGui::Render();

			drawFrame();
		}
		vkDeviceWaitIdle(VulkanCore::getDevice());

		for (uint32_t i = 0; i < viewPort.m_ViewportImageViews.size(); i++)
			ImGui_ImplVulkan_RemoveTexture(sceneTexture[i]);
	}

private:
	VkResult err;
	VulkanCore engineCore;
	EditorCamera editorCamera;
	ViewPort viewPort;
	MousePick mousePick;
	Outline outline;
	UIManager uiManager;
	VkExtent2D viewportExtent;
	std::vector<VkDescriptorSet> sceneTexture;

	static void check_vk_result(VkResult err) {
		if (err == 0)
			return;
		fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
		if (err < 0)
			abort();
	}

	void init() {
		// Initialize UI Manager (ImGui)
		uiManager.init(&engineCore);
	}

	void recordImguiCommandBuffer(VkCommandBuffer commandBuffer, uint32_t ImageIndex) {
		uiManager.recordCommandBuffer(commandBuffer, ImageIndex);
	}

	void recreateRenderPasses() {
		vkDeviceWaitIdle(VulkanCore::getDevice());
		for (uint32_t i = 0; i < viewPort.m_ViewportImageViews.size(); i++)
			ImGui_ImplVulkan_RemoveTexture(sceneTexture[i]);

		uiManager.recreateFramebuffers();
		mousePick.recreateMousePick();
		viewPort.recreateViewport(mousePick.getMousePickExtent());
		outline.recreateOutline(
			mousePick.getMousePickImageViews(), viewPort.m_ViewportImageViews, mousePick.getMousePickExtent());

		for (uint32_t i = 0; i < viewPort.m_ViewportImageViews.size(); i++)
			sceneTexture[i] = ImGui_ImplVulkan_AddTexture(engineCore.getTextureSampler(),
														  outline.outlineColorImageViews[i],
														  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	void cleanup() {
		uiManager.cleanup();
	}

	void inputProcess() {
		editorCamera.inputProcess(mousePick);
	}

	void changeImGuizmoStyle() {
		ImGuizmo::Style& style = ImGuizmo::GetStyle();
		style.HatchedAxisLineThickness = 0.0f;
		style.CenterCircleSize = 7.0f;
		ImGuizmo::AllowAxisFlip(false);
	}
};

int main() {
	VulkanEngine app;

	try {
		app.run();
	} catch (const std::exception& e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
