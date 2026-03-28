#include <array>
#include <corecrt_terminate.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
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
#include "core/input/Input.h"
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
#include "managers/ScriptCompiler.h"
#include "managers/ScriptPluginLoader.h"
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
		Core::Input::init(eventBus);

		engineCore.initVulkan();

		// Initialize ResourceContext managers (create pools/buffers)
		resourceContext.init(&engineCore);
		resourceContext.getLightManager().initDescriptorResources(engineCore.getDevice(),
																  engineCore.getGlobalDescriptorPool());

		// IBL must be initialized BEFORE loadDefaults()
		ibl.init(static_cast<Renderer::VulkanDevice*>(&resourceContext.getDevice()),
				 VulkanCore::getCommandPool(),
				 "common/texture/skybox/environment.hdr");

		// Initialize resource managers (textures, materials, lights)
		// ResourceContext constructor created the managers, now load content
		resourceContext.loadDefaults();
		resourceContext.getMeshManager().loadDefaults(VulkanCore::getCommandPool(), VulkanCore::getGraphicsQueue());

		// Link AssetBrowser to InspectorUi (circular dependency resolution)
		inspector.setAssetBrowser(&assetBrowser);

		SceneRenderer::init(&resourceContext);
		SceneRenderer::initDescriptorResources(
			engineCore.getDevice(), engineCore.getGlobalDescriptorPool(), VulkanCore::getPhysicalDevice());

		// Create graphics pipeline AFTER managers have initialized their layouts
		engineCore.createGraphicsPipeline(resourceContext.getLightManager().getDescriptorSetLayout(),
										  resourceContext.getMaterialManager().getStandardLayout(),
										  SceneRenderer::getPerObjectDescriptorSetLayout());

		// Update VulkanDevice abstraction with the now-valid pipeline handles
		auto* vulkanDev = static_cast<Renderer::VulkanDevice*>(&resourceContext.getDevice());
		vulkanDev->updatePipeline(engineCore.getPipeline(), engineCore.getPipelineLayout());
		vulkanDev->updateDynamicAlignment(SceneRenderer::getDynamicAlignment());

		mousePick.init(static_cast<Renderer::VulkanDevice*>(&resourceContext.getDevice()), &resourceContext);
		viewPort.init(static_cast<Renderer::VulkanDevice*>(&resourceContext.getDevice()),
					  mousePick.getMousePickExtent(),
					  ibl.getEnvCubemapImageView(),
					  ibl.getEnvCubemapSampler(),
					  false /*editorViewport*/);
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

		// Game viewport — same size as editor for now (can be independent)
		gameViewPort.init(static_cast<Renderer::VulkanDevice*>(&resourceContext.getDevice()),
						  mousePick.getMousePickExtent(),
						  ibl.getEnvCubemapImageView(),
						  ibl.getEnvCubemapSampler(),
						  true /*isGameViewport*/);
		gameHdrTonemap.init(static_cast<Renderer::VulkanDevice*>(&resourceContext.getDevice()),
							gameViewPort.hdrResolveImageView,
							mousePick.getMousePickExtent().width,
							mousePick.getMousePickExtent().height);
		editorCamera.init(
			static_cast<Renderer::VulkanDevice*>(&resourceContext.getDevice()), &resourceContext, &inspector);

		editorMenu.loadLastScene();

		init();

		changeImGuizmoStyle();
		mainLoop();

		gameHdrTonemap.cleanup();
		gameViewPort.cleanup();
		hdrTonemap.cleanup();
		outline.cleanup();
		viewPort.cleanup();
		mousePick.cleanup();
		cleanup();

		// ResourceContext cleanup handled by destructor
		resourceContext.cleanup();

		// Shutdown script system AFTER scenes/entities are destroyed.
		// Otherwise ScriptComponent instances may be deleted after plugin unload.
		ScriptCompiler::shutdown();
		ScriptPluginLoader::unloadAll();

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
		uint32_t frame = VulkanCore::getCurrentFrame();
		editorCamera.updateUniformBuffer(frame,
										 SceneRenderer::getUniformBuffersMapped()[frame],
										 VulkanCore::getEditorGlobalBuffersMapped()[frame],
										 VulkanCore::getGameGlobalBuffersMapped()[frame],
										 lightSpaceMatrices,
										 lightPos_farPlane);

		// --- 1. SHADOW PASS ---
		shadowMap.recordShadowCommandBuffer(VulkanCore::getCurrentFrame(), lightSpaceMatrices, lightPos_farPlane);
		mousePick.recordMousePickCommandBuffer(mousePick.mousePickCommandBuffers[VulkanCore::getCurrentFrame()],
											   imageIndex);

		VkDescriptorSet editorGlobalSet = VulkanCore::getEditorGlobalDescriptorSets()[frame];
		VkDescriptorSet gameGlobalSet = VulkanCore::getGameGlobalDescriptorSets()[frame];

		viewPort.recordViewportCommandBuffer(
			viewPort.m_ViewportCommandBuffers[frame], imageIndex, lightSpaceMatrices[0], editorGlobalSet);

		gameViewPort.recordViewportCommandBuffer(
			gameViewPort.m_ViewportCommandBuffers[frame], imageIndex, lightSpaceMatrices[0], gameGlobalSet);

		// Assuming an exposure of 1.0 for now, could be passed from UI
		hdrTonemap.recordHdrCommandBuffer(frame, imageIndex, exposure);
		gameHdrTonemap.recordHdrCommandBuffer(frame, imageIndex, exposure);

		outline.recordOutlineCommandBuffer(outline.outlineCommandBuffers[frame], imageIndex);

		recordImguiCommandBuffer(uiManager.getCommandBuffers()[frame], imageIndex);

		std::array<VkCommandBuffer, 8> submitCommandBuffers = {
			shadowMap.getCommandBuffer(frame),
			mousePick.mousePickCommandBuffers[frame],
			viewPort.m_ViewportCommandBuffers[frame],
			static_cast<VkCommandBuffer>(hdrTonemap.getCommandBuffer(frame)),
			gameViewPort.m_ViewportCommandBuffers[frame],
			static_cast<VkCommandBuffer>(gameHdrTonemap.getCommandBuffer(frame)),
			outline.outlineCommandBuffers[frame],
			uiManager.getCommandBuffers()[frame],
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

		gameSceneTexture.resize(gameHdrTonemap.getLdrImageViewCount());
		for (uint32_t i = 0; i < gameHdrTonemap.getLdrImageViewCount(); i++)
			gameSceneTexture[i] =
				ImGui_ImplVulkan_AddTexture(engineCore.getTextureSampler(),
											static_cast<VkImageView>(gameHdrTonemap.getLdrImageView(i)),
											VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		while (!glfwWindowShouldClose(engineCore.getWindow())) {
			double currentFrame = glfwGetTime();
			deltaTime = currentFrame - lastFrame;
			lastFrame = currentFrame;
			Core::Input::beginFrame();
			glfwPollEvents();

			// Tick scene (scripts, future physics, etc.) every frame
			if (Scene* activeScene = resourceContext.getSceneManager().getActiveScene()) {
				activeScene->onUpdate(static_cast<float>(deltaTime));
			}

			if (engineCore.getSwapChainRecreated()) {
				recreateEditorViewportResources();
				recreateGameViewportResources();

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

			ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_MenuBar);

			if (ImGui::BeginMenuBar()) {
				const ImGuiStyle& style = ImGui::GetStyle();
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(style.FramePadding.x, style.FramePadding.y + 1.0f));
				const bool isLocalMode = (EditorCamera::getGizmoMode() == ImGuizmo::LOCAL);
				const char* currentModeLabel = isLocalMode ? "Local" : "Global";
				if (ImGui::BeginMenu(currentModeLabel)) {
					if (ImGui::MenuItem("Local", nullptr, isLocalMode)) {
						EditorCamera::setGizmoMode(ImGuizmo::LOCAL);
					}
					if (ImGui::MenuItem("Global", nullptr, !isLocalMode)) {
						EditorCamera::setGizmoMode(ImGuizmo::WORLD);
					}
					ImGui::EndMenu();
				}
				ImGui::PopStyleVar(2);
				ImGui::EndMenuBar();
			}

			ImVec2 viewportSize = ImGui::GetContentRegionAvail();

			uint32_t viewportWidth = (viewportSize.x > 0.0f) ? static_cast<uint32_t>(viewportSize.x) : 0;
			uint32_t viewportHeight = (viewportSize.y > 0.0f) ? static_cast<uint32_t>(viewportSize.y) : 0;

			this->editorViewportExtent = VkExtent2D{viewportWidth, viewportHeight};
			EditorCamera::setEditorExtent(this->editorViewportExtent);

			if (editorViewportExtent.width > 0 && editorViewportExtent.height > 0 &&
				(editorViewportExtent.width != mousePick.mousePickExtent.width ||
				 editorViewportExtent.height != mousePick.mousePickExtent.height)) {
				mousePick.mousePickExtent = editorViewportExtent;
				recreateEditorViewportResources();
			}

			inputProcess();

			ImDrawList* draw_list = ImGui::GetWindowDrawList();

			// Get the current cursor screen position as a reference point
			ImVec2 p = ImGui::GetCursorScreenPos();

			ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

			ImGui::Image((ImTextureID)sceneTexture[imageIndex], ImVec2{viewportPanelSize.x, viewportPanelSize.y});

			ImGui::SetCursorScreenPos(p);

			EditorCamera::drawGuizmo();

			const bool editorWindowActive = ImGui::IsWindowFocused() || ImGui::IsWindowHovered();

			ImGui::End();
			ImGui::PopStyleVar(2);

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
			ImGui::Begin("Game");

			// Switch to the Game tab automatically when Play is pressed
			Scene* activeScene = resourceContext.getSceneManager().getActiveScene();
			if (activeScene && activeScene->getState() == SceneState::Play && !wasPlayingLastFrame) {
				ImGui::SetWindowFocus();
			}

			const bool gameWindowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
			const bool gameWindowHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
			const bool gameWindowActive = gameWindowFocused || gameWindowHovered;
			EditorCamera::useGameCameraView = gameWindowActive && !editorWindowActive;

			const bool isPlaying = (activeScene && activeScene->getState() == SceneState::Play);
			const bool enteringPlay = isPlaying && !wasPlayingLastFrame;
			const bool appFocused = glfwGetWindowAttrib(engineCore.getWindow(), GLFW_FOCUSED) == GLFW_TRUE;
			const bool escapePressed = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
			const bool gameCaptureClicked = isPlaying && gameWindowHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

			if (enteringPlay && appFocused) {
				cursorCaptured = true;
			}

			if (!isPlaying || !appFocused || escapePressed) {
				cursorCaptured = false;
			} else if (gameCaptureClicked) {
				cursorCaptured = true;
			}

			if (escapePressed) {
				Core::Input::clearAll();
			}

			const bool gameInputEnabled = isPlaying && cursorCaptured && appFocused;
			Core::Input::setGameplayInputEnabled(gameInputEnabled);
			if (gameInputEnabled) {
				glfwSetInputMode(engineCore.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			} else {
				glfwSetInputMode(engineCore.getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			}
			wasPlayingLastFrame = isPlaying;

			ImVec2 gameSize = ImGui::GetContentRegionAvail();
			uint32_t gameWidth = (gameSize.x > 0.0f) ? static_cast<uint32_t>(gameSize.x) : 0;
			uint32_t gameHeight = (gameSize.y > 0.0f) ? static_cast<uint32_t>(gameSize.y) : 0;

			VkExtent2D newGameExtent = VkExtent2D{gameWidth, gameHeight};
			EditorCamera::setGameExtent(newGameExtent);

			if (newGameExtent.width > 0 && newGameExtent.height > 0 &&
				(newGameExtent.width != gameViewportExtent.width ||
				 newGameExtent.height != gameViewportExtent.height)) {
				this->gameViewportExtent = newGameExtent;
				recreateGameViewportResources();
			}

			ImVec2 gamePanelSize = ImGui::GetContentRegionAvail();
			ImGui::Image((ImTextureID)gameSceneTexture[imageIndex], ImVec2{gamePanelSize.x, gamePanelSize.y});

			if (isPlaying) {
				ImDrawList* gameDrawList = ImGui::GetWindowDrawList();
				const ImVec2 imageMin = ImGui::GetItemRectMin();
				if (!gameInputEnabled) {
					const ImVec2 pad(10.0f, 10.0f);
					const ImVec2 textPos = ImVec2(imageMin.x + pad.x, imageMin.y + pad.y);
					const ImU32 textCol = IM_COL32(230, 230, 230, 255);
					const ImU32 shadowCol = IM_COL32(0, 0, 0, 180);
					gameDrawList->AddText(ImVec2(textPos.x + 1.0f, textPos.y + 1.0f), shadowCol, "Click Game view to capture mouse");
					gameDrawList->AddText(textPos, textCol, "Click Game view to capture mouse");
					const ImVec2 textPos2 = ImVec2(textPos.x, textPos.y + ImGui::GetTextLineHeightWithSpacing());
					gameDrawList->AddText(ImVec2(textPos2.x + 1.0f, textPos2.y + 1.0f), shadowCol, "Press Esc to release");
					gameDrawList->AddText(textPos2, textCol, "Press Esc to release");
				} else {
					const ImVec2 textPos = ImVec2(imageMin.x + 10.0f, imageMin.y + 10.0f);
					const ImU32 textCol = IM_COL32(200, 255, 200, 220);
					const ImU32 shadowCol = IM_COL32(0, 0, 0, 160);
					gameDrawList->AddText(ImVec2(textPos.x + 1.0f, textPos.y + 1.0f), shadowCol, "Esc to release mouse");
					gameDrawList->AddText(textPos, textCol, "Esc to release mouse");
				}
			}

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
		for (uint32_t i = 0; i < gameHdrTonemap.getLdrImageViewCount(); i++)
			ImGui_ImplVulkan_RemoveTexture(gameSceneTexture[i]);
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
	Renderer::VulkanHdr gameHdrTonemap;
	Outline outline;
	UIManager uiManager;
	VkExtent2D editorViewportExtent;
	VkExtent2D gameViewportExtent;
	ViewPort gameViewPort;
	std::vector<VkDescriptorSet> sceneTexture;
	std::vector<VkDescriptorSet> gameSceneTexture;
	Renderer::VulkanShadowMap shadowMap;
	VulkanIBL ibl;
	float exposure = 1.0f;
	bool cursorCaptured = false;
	bool wasPlayingLastFrame = false;

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

		resourceContext.getLightManager().updateDescriptorSets(&shadowMap,
															   ibl.getIrradianceImageView(),
															   ibl.getIrradianceSampler(),
															   ibl.getPrefilterImageView(),
															   ibl.getPrefilterSampler(),
															   ibl.getBrdfLutImageView(),
															   ibl.getBrdfLutSampler());

		// --- Script System ---
		// Built-in scripts self-register via the SCRIPT macro.
		// Scripts are loaded on-demand via ScriptCompiler + ScriptPluginLoader
		// when .h files are dragged into the Inspector.

		// Setup the per-script compiler (auto-detects cl.exe via vswhere)
		{
			char exeBuf[MAX_PATH] = {};
			GetModuleFileNameA(nullptr, exeBuf, MAX_PATH);
			std::filesystem::path solutionRoot = std::filesystem::path(exeBuf).parent_path() / ".." / "..";
			solutionRoot = std::filesystem::weakly_canonical(solutionRoot);

			std::filesystem::path projectsDir = solutionRoot / "projects";
			std::string engineRoot = (solutionRoot / "Vulkan_Engine").string();
			std::string engineLib = (solutionRoot / "x64" / "Debug" / "Vulkan_Engine.lib").string();
			std::string glmInclude = (solutionRoot / "Vulkan_Engine" / "vcpkg_installed" / "x64-windows" / "x64-windows" / "include").string();

			ScriptCompiler::setupConfig(projectsDir.string(), engineRoot, engineLib, glmInclude);
		}
	}

	void recordImguiCommandBuffer(VkCommandBuffer commandBuffer, uint32_t ImageIndex) {
		uiManager.recordCommandBuffer(commandBuffer, ImageIndex);
	}

	void recreateEditorViewportResources() {
		vkDeviceWaitIdle(VulkanCore::getDevice());

		// Update VulkanDevice with the new swapchain properties if needed
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

	void recreateGameViewportResources() {
		vkDeviceWaitIdle(VulkanCore::getDevice());

		for (uint32_t i = 0; i < gameHdrTonemap.getLdrImageViewCount(); i++)
			ImGui_ImplVulkan_RemoveTexture(gameSceneTexture[i]);

		gameViewPort.recreateViewport(gameViewportExtent);
		gameHdrTonemap.recreateHdr(
			gameViewPort.hdrResolveImageView, gameViewportExtent.width, gameViewportExtent.height);

		for (uint32_t i = 0; i < gameHdrTonemap.getLdrImageViewCount(); i++)
			gameSceneTexture[i] =
				ImGui_ImplVulkan_AddTexture(engineCore.getTextureSampler(),
											static_cast<VkImageView>(gameHdrTonemap.getLdrImageView(i)),
											VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	void cleanup() {
		uiManager.cleanup();
		shadowMap.cleanup();
		ibl.cleanup();
	}

	void inputProcess() {
		editorCamera.inputProcess(mousePick);
		ScriptCompiler::tick(); // Deliver async compile callbacks on the main thread
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
