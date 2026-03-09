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

#include "glm/ext/matrix_float4x4.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/gtx/quaternion.hpp"

#define IMGUI_DEFINE_MATH_OPERATORS
#define fontPath "C:\\Windows\\Fonts\\segoeuisl.ttf" // Windows UI font
#define boldFontPath "C:\\Windows\\Fonts\\segoeuib.ttf" // Windows UI bold font

#include <iostream>

#include "components/LightComponent.h"
#include "components/MeshComponent.h"
#include "context/ResourceContext.h"
#include "core/events/EventBus.h"
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
#include "managers/MeshManager.h"
#include "managers/ProjectSerializer.h"
#include "managers/SceneManager.h"
#include "postprocess/outline.h"
#include "renderer/vulkan/VulkanDevice.h"
#include "renderer/vulkan/VulkanHdr.h"
#include "renderer/vulkan/VulkanIBL.h"
#include "renderer/vulkan/VulkanShadowMap.h"
#include "SceneRenderer.h"
#include "ui/AssetBrowser.h"
#include "ui/EditorMenu.h"
#include "ui/InspectorUi.h"
#include "ui/SceneUi.h"
#include "ui/UIManager.h"

// GLOBAL VARIABLES
double deltaTime = 0.0; // Time between current frame and last frame
double lastFrame = 0.0; // Time of last frame

class VulkanEngine {
public:
	VulkanEngine()
		: inspector(resourceContext), assetBrowser(resourceContext, inspector), sceneUi(resourceContext, inspector),
		  editorMenu(resourceContext) {
	}

	Core::EventBus eventBus;

	void run() {
		// Initialize Vulkan
		engineCore.initWindow();
		editorMenu.setWindow(engineCore.getWindow());

		// Wire up Event System
		engineCore.setEventBus(&eventBus);

		engineCore.initVulkan();

		// Initialize ResourceContext managers (create pools/buffers)
		resourceContext.init(&engineCore);

		// IBL must be initialized BEFORE loadDefaults() so that when materials are
		// first created their descriptor set slot 6 (irradianceMap) receives the
		// real cubemap view instead of a 2D fallback placeholder.
		ibl.init(static_cast<Renderer::VulkanDevice*>(&resourceContext.getDevice()),
				 VulkanCore::getCommandPool(),
				 "common/texture/skybox/environment.hdr");
		resourceContext.getMaterialManager().setIrradianceMap(ibl.getIrradianceImageView(), ibl.getIrradianceSampler());
		resourceContext.getMaterialManager().setSpecularIBL(
			ibl.getPrefilterImageView(), ibl.getPrefilterSampler(), ibl.getBrdfLutImageView(), ibl.getBrdfLutSampler());

		// Initialize resource managers (textures, materials, lights)
		// ResourceContext constructor created the managers, now load content
		resourceContext.loadDefaults();
		resourceContext.getMeshManager().loadDefaults(VulkanCore::getCommandPool(), VulkanCore::getGraphicsQueue());

		// Link AssetBrowser to InspectorUi (circular dependency resolution)
		inspector.setAssetBrowser(&assetBrowser);

		SceneRenderer::init(&resourceContext);
		mousePick.init(static_cast<Renderer::VulkanDevice*>(&resourceContext.getDevice()), &resourceContext);
		viewPort.init(static_cast<Renderer::VulkanDevice*>(&resourceContext.getDevice()),
					  mousePick.getMousePickExtent(),
					  ibl.getEnvCubemapImageView(),
					  ibl.getEnvCubemapSampler());
		hdrTonemap.init(static_cast<Renderer::VulkanDevice*>(&resourceContext.getDevice()),
						viewPort.hdrResolveImageView,
						mousePick.getMousePickExtent().width,
						mousePick.getMousePickExtent().height);
		std::vector<VkImageView> ldrImageViews(hdrTonemap.getLdrImageViewCount());
		for (uint32_t i = 0; i < hdrTonemap.getLdrImageViewCount(); i++) {
			ldrImageViews[i] = static_cast<VkImageView>(hdrTonemap.getLdrImageView(i));
		}

		outline.init(static_cast<Renderer::VulkanDevice*>(&resourceContext.getDevice()),
					 mousePick.getMousePickImageViews(),
					 ldrImageViews,
					 mousePick.getMousePickExtent());
		editorCamera.init(
			static_cast<Renderer::VulkanDevice*>(&resourceContext.getDevice()), &resourceContext, &inspector);

		editorMenu.loadLastScene();

		init();

		changeImGuizmoStyle();
		mainLoop();

		hdrTonemap.cleanup();
		outline.cleanup();
		viewPort.cleanup();
		mousePick.cleanup();
		cleanup();

		// ResourceContext cleanup handled by destructor
		resourceContext.cleanup();

		engineCore.cleanup();
	}

	void drawFrame(uint32_t imageIndex) {
		// (TEMPORARY)
		// Update simple single light (for now static directional light)
		glm::mat4 lightSpaceMatrices[6] = {
			glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f)};
		glm::vec4 lightPos_farPlane(0.0f);
		{
			uint32_t frame = VulkanCore::getCurrentFrame();
			// Find the first LightComponent in the active scene. If none found,
			// upload a zero-intensity light so the scene is dark.
			LightComponent::LightUniform lu{};
			bool foundLight = false;
			Scene* scene = resourceContext.getSceneManager().getActiveScene();
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
				lu.colorIntensity = glm::vec4(0.0f);
			} else {
				// For directional lights (type 0), pass camera world position in positionType.xyz
				if (static_cast<int>(lu.positionType.w) == 0) {
					glm::vec3 cameraPos = EditorCamera::cameraPos;
					lu.positionType = glm::vec4(cameraPos, 0.0f);
				}

				// Defer entirely to the ShadowMap interface to calculate complex graphics projection math
				shadowMap.calculateShadowMatrices(
					lu.positionType, lu.direction, lu.far_plane, lu.outerCutOff, lightSpaceMatrices, lightPos_farPlane);
			}
			resourceContext.getLightManager().updateLight(frame, lu);
		}

		// Update per-frame camera UBO first so command buffer recordings use up-to-date data
		editorCamera.updateUniformBuffer(VulkanCore::getCurrentFrame(),
										 engineCore.getUniformBuffersMapped()[VulkanCore::getCurrentFrame()],
										 lightSpaceMatrices,
										 lightPos_farPlane);

		// --- 1. SHADOW PASS ---
		shadowMap.recordShadowCommandBuffer(VulkanCore::getCurrentFrame(), lightSpaceMatrices, lightPos_farPlane);
		mousePick.recordMousePickCommandBuffer(mousePick.mousePickCommandBuffers[VulkanCore::getCurrentFrame()],
											   imageIndex);

		viewPort.recordViewportCommandBuffer(
			viewPort.m_ViewportCommandBuffers[VulkanCore::getCurrentFrame()], imageIndex, lightSpaceMatrices[0]);

		// Assuming an exposure of 1.0 for now, could be passed from UI
		hdrTonemap.recordHdrCommandBuffer(VulkanCore::getCurrentFrame(), imageIndex, exposure);

		outline.recordOutlineCommandBuffer(outline.outlineCommandBuffers[VulkanCore::getCurrentFrame()], imageIndex);

		recordImguiCommandBuffer(uiManager.getCommandBuffers()[VulkanCore::getCurrentFrame()], imageIndex);

		std::array<VkCommandBuffer, 6> submitCommandBuffers = {
			shadowMap.getCommandBuffer(VulkanCore::getCurrentFrame()),
			mousePick.mousePickCommandBuffers[VulkanCore::getCurrentFrame()],
			viewPort.m_ViewportCommandBuffers[VulkanCore::getCurrentFrame()],
			static_cast<VkCommandBuffer>(hdrTonemap.getCommandBuffer(VulkanCore::getCurrentFrame())),
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

		VkResult result = vkQueuePresentKHR(VulkanCore::getPresentQueue(), &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || engineCore.getFramebufferResized()) {
			engineCore.setFramebufferResized(false);
			engineCore.recreateSwapChain();
		} else if (result != VK_SUCCESS) {
			throw std::runtime_error("failed to present swap chain image!");
		}

		engineCore.setCurrentFrame((VulkanCore::getCurrentFrame() + 1) % MAX_FRAMES_IN_FLIGHT);
	}

	void mainLoop() {
		sceneTexture.resize(hdrTonemap.getLdrImageViewCount());
		for (uint32_t i = 0; i < hdrTonemap.getLdrImageViewCount(); i++)
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

			int width = 0, height = 0;
			glfwGetFramebufferSize(engineCore.getWindow(), &width, &height);
			// Early pause if minimized
			if (width == 0 || height == 0) {
				glfwWaitEvents();
				continue;
			}

			// Acquire image at start of frame
			vkWaitForFences(VulkanCore::getDevice(),
							1,
							&engineCore.getInFlightFences()[VulkanCore::getCurrentFrame()],
							VK_TRUE,
							UINT64_MAX);

			resourceContext.getMaterialManager().cleanupPendingResources(VulkanCore::getCurrentFrame());

			uint32_t imageIndex;
			VkResult result =
				vkAcquireNextImageKHR(VulkanCore::getDevice(),
									  engineCore.getSwapChain(),
									  UINT64_MAX,
									  engineCore.getImageAvailableSemaphores()[VulkanCore::getCurrentFrame()],
									  VK_NULL_HANDLE,
									  &imageIndex);

			if (result == VK_ERROR_OUT_OF_DATE_KHR) {
				engineCore.recreateSwapChain();
				continue;
			} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
				throw std::runtime_error("failed to acquire swap chain image!");
			}

			vkResetFences(VulkanCore::getDevice(), 1, &engineCore.getInFlightFences()[VulkanCore::getCurrentFrame()]);
			vkResetCommandBuffer(engineCore.getCommandBuffers()[VulkanCore::getCurrentFrame()], 0);

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

			ImGui::Image((ImTextureID)sceneTexture[imageIndex], ImVec2{viewportPanelSize.x, viewportPanelSize.y});

			ImGui::SetCursorScreenPos(p);

			EditorCamera::drawGuizmo();

			ImGui::End();
			ImGui::PopStyleVar(2);

			ImGui::Begin("Renderer Settings");
			ImGui::SliderFloat("Exposure", &exposure, 0.1f, 10.0f);
			ImGui::End();

			sceneUi.render();

			inspector.render();

			assetBrowser.render();

			editorMenu.render();

			ImGui::Render();

			drawFrame(imageIndex);
		}
		vkDeviceWaitIdle(VulkanCore::getDevice());

		for (uint32_t i = 0; i < hdrTonemap.getLdrImageViewCount(); i++)
			ImGui_ImplVulkan_RemoveTexture(sceneTexture[i]);
	}

private:
	VkResult err;
	VulkanCore engineCore;
	ResourceContext resourceContext;
	InspectorUi inspector;
	AssetBrowser assetBrowser;
	SceneUi sceneUi;
	EditorMenu editorMenu;
	EditorCamera editorCamera;
	ViewPort viewPort;
	MousePick mousePick;
	Renderer::VulkanHdr hdrTonemap;
	Outline outline;
	UIManager uiManager;
	VkExtent2D viewportExtent;
	std::vector<VkDescriptorSet> sceneTexture;
	Renderer::VulkanShadowMap shadowMap;
	VulkanIBL ibl;
	float exposure = 1.0f;

	static void check_vk_result(VkResult err) {
		if (err == 0)
			return;
		fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
		if (err < 0)
			abort();
	}

	void init() {
		// Initialize UI Manager (ImGui)
		uiManager.init(static_cast<Renderer::VulkanDevice*>(&resourceContext.getDevice()),
					   engineCore.getSwapChainImageViews());
		shadowMap.init(static_cast<Renderer::VulkanDevice*>(&resourceContext.getDevice()),
					   1024,
					   1024); // Initialize as PointLight to ensure 6 layers and cube view
		resourceContext.getMaterialManager().setShadowMap(&shadowMap);
	}

	void recordImguiCommandBuffer(VkCommandBuffer commandBuffer, uint32_t ImageIndex) {
		uiManager.recordCommandBuffer(commandBuffer, ImageIndex);
	}

	void recreateRenderPasses() {
		vkDeviceWaitIdle(VulkanCore::getDevice());

		// Update VulkanDevice with the new swapchain properties
		static_cast<Renderer::VulkanDevice*>(&resourceContext.getDevice())
			->updateSwapchain(engineCore.getSwapChainExtent(),
							  static_cast<uint32_t>(engineCore.getSwapChainImageViews().size()));

		for (uint32_t i = 0; i < hdrTonemap.getLdrImageViewCount(); i++)
			ImGui_ImplVulkan_RemoveTexture(sceneTexture[i]);

		uiManager.recreateFramebuffers(engineCore.getSwapChainImageViews());
		mousePick.recreateMousePick();
		viewPort.recreateViewport(mousePick.getMousePickExtent());
		hdrTonemap.recreateHdr(
			viewPort.hdrResolveImageView, mousePick.getMousePickExtent().width, mousePick.getMousePickExtent().height);
		std::vector<VkImageView> ldrImageViews(hdrTonemap.getLdrImageViewCount());
		for (uint32_t i = 0; i < hdrTonemap.getLdrImageViewCount(); i++) {
			ldrImageViews[i] = static_cast<VkImageView>(hdrTonemap.getLdrImageView(i));
		}

		outline.recreateOutline(mousePick.getMousePickImageViews(), ldrImageViews, mousePick.getMousePickExtent());

		for (uint32_t i = 0; i < hdrTonemap.getLdrImageViewCount(); i++)
			sceneTexture[i] = ImGui_ImplVulkan_AddTexture(engineCore.getTextureSampler(),
														  outline.outlineColorImageViews[i],
														  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	void cleanup() {
		uiManager.cleanup();
		shadowMap.cleanup();
		ibl.cleanup();
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
