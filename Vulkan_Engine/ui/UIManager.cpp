#include "UIManager.h"

#include <corecrt_terminate.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <stdio.h>
#include <vector>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "renderer/vulkan/VulkanDevice.h"
#include "vulkan/vulkan_core.h"

// Font paths
static const char* fontPath = "C:/Windows/Fonts/segoeui.ttf";
static const char* boldFontPath = "C:/Windows/Fonts/segoeuib.ttf";

void UIManager::checkVkResult(VkResult err) {
	if (err == 0)
		return;
	fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
	if (err < 0)
		abort();
}

void UIManager::init(Renderer::VulkanDevice* device, const std::vector<VkImageView>& swapchainImageViews) {
	this->vulkanDevice = device;

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // IF using Docking Branch
	io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

	// Load fonts
	FILE* f = nullptr;
	if (fopen_s(&f, fontPath, "rb") != 0 || !f) {
		io.Fonts->AddFontDefault();
	} else {
		fclose(f);

		ImFontConfig fontCfg;
		fontCfg.SizePixels = 16.0f;

		io.Fonts->AddFontFromFileTTF(fontPath, fontCfg.SizePixels, &fontCfg, io.Fonts->GetGlyphRangesDefault());
	}

	if (fopen_s(&f, boldFontPath, "rb") != 0 || !f) {
		io.Fonts->AddFontDefault();
	} else {
		fclose(f);

		ImFontConfig fontCfg;
		fontCfg.SizePixels = 16.0f;

		io.Fonts->AddFontFromFileTTF(boldFontPath, fontCfg.SizePixels, &fontCfg, io.Fonts->GetGlyphRangesDefault());
	}

	setupImGuiStyle();
	createDescriptorPool();
	createRenderPass();
	createCommandPool();
	createCommandBuffers(static_cast<uint32_t>(swapchainImageViews.size()));
	createFramebuffers(swapchainImageViews);

	// Setup Platform/Renderer backends
	ImGui_ImplGlfw_InitForVulkan(vulkanDevice->getWindow(), true);

	uint32_t imageCount = vulkanDevice->getMinImageCount() + 1;

	ImGui_ImplVulkan_InitInfo init_info{};
	init_info.Instance = vulkanDevice->getInstance();
	init_info.PhysicalDevice = vulkanDevice->getPhysicalDevice();
	init_info.Device = vulkanDevice->getDevice();
	init_info.QueueFamily = vulkanDevice->getGraphicsQueueFamily();
	init_info.Queue = vulkanDevice->getGraphicsQueue();
	init_info.PipelineCache = VK_NULL_HANDLE;
	init_info.DescriptorPool = imguiDescriptorPool;
	init_info.PipelineInfoMain.RenderPass = imGuiRenderPass;
	init_info.PipelineInfoMain.Subpass = 0;
	init_info.MinImageCount = imageCount;
	init_info.ImageCount = imageCount;
	init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	init_info.Allocator = nullptr;
	init_info.CheckVkResultFn = checkVkResult;

	ImGui_ImplVulkan_Init(&init_info);
}

void UIManager::beginFrame() {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void UIManager::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	err = vkBeginCommandBuffer(commandBuffer, &beginInfo);
	checkVkResult(err);

	VkRenderPassBeginInfo info{};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	info.renderPass = imGuiRenderPass;
	info.framebuffer = imGuiFramebuffers[imageIndex];
	info.renderArea.extent.width = vulkanDevice->getSwapChainExtent().width;
	info.renderArea.extent.height = vulkanDevice->getSwapChainExtent().height;
	info.clearValueCount = 1;
	VkClearValue clearValue{};
	clearValue.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
	info.clearValueCount = 1;
	info.pClearValues = &clearValue;
	vkCmdBeginRenderPass(commandBuffer, &info, VK_SUBPASS_CONTENTS_INLINE);

	// Record Imgui Draw Data and draw funcs into command buffer
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

	vkCmdEndRenderPass(commandBuffer);
	err = vkEndCommandBuffer(commandBuffer);
	checkVkResult(err);
}

void UIManager::recreateFramebuffers(const std::vector<VkImageView>& swapchainImageViews) {
	cleanupFramebuffers();
	createFramebuffers(swapchainImageViews);
}

void UIManager::cleanup() {
	for (auto framebuffer : imGuiFramebuffers) {
		vkDestroyFramebuffer(vulkanDevice->getDevice(), framebuffer, nullptr);
	}

	vkDestroyRenderPass(vulkanDevice->getDevice(), imGuiRenderPass, nullptr);

	vkFreeCommandBuffers(vulkanDevice->getDevice(),
						 imGuiCommandPool,
						 static_cast<uint32_t>(imGuiCommandBuffers.size()),
						 imGuiCommandBuffers.data());
	vkDestroyCommandPool(vulkanDevice->getDevice(), imGuiCommandPool, nullptr);

	// Resources to destroy when the program ends
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	vkDestroyDescriptorPool(vulkanDevice->getDevice(), imguiDescriptorPool, nullptr);
}

void UIManager::setupImGuiStyle() {
	ImGuiStyle& style = ImGui::GetStyle();
	ImVec4* colors = style.Colors;

	colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.94f);
	colors[ImGuiCol_Border] = ImVec4(0.0f, 0.0f, 0.0f, 0.99f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.54f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.38f, 0.51f, 0.51f, 0.80f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.03f, 0.03f, 0.04f, 0.67f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.01f, 0.01f, 0.01f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.07f, 0.07f, 0.07f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.17f, 0.17f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.30f, 0.60f, 0.10f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.30f, 0.60f, 0.10f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.43f, 0.90f, 0.11f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.21f, 0.22f, 0.23f, 0.40f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.38f, 0.51f, 0.51f, 0.80f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.54f, 0.55f, 0.55f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.38f, 0.51f, 0.51f, 0.80f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.03f, 0.03f, 0.03f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.20f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.23f, 0.23f, 0.24f, 0.80f);
	colors[ImGuiCol_Tab] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
	colors[ImGuiCol_TabSelected] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
	colors[ImGuiCol_TabSelectedOverline] = ImVec4(0.13f, 0.78f, 0.07f, 1.00f);
	colors[ImGuiCol_TabDimmed] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
	colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
	colors[ImGuiCol_DockingPreview] = ImVec4(0.26f, 0.59f, 0.98f, 0.70f);
	colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.14f, 0.87f, 0.05f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.30f, 0.60f, 0.10f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.23f, 0.78f, 0.02f, 1.00f);
	colors[ImGuiCol_TableHeaderBg] = ImVec4(0.27f, 0.27f, 0.27f, 1.00f);
	colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
	colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);
	colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.46f, 0.47f, 0.46f, 0.06f);
	colors[ImGuiCol_TextLink] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavCursor] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.78f, 0.69f, 0.69f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

	style.WindowBorderSize = 0.0f;
	style.WindowRounding = 4.0f;
	style.FrameRounding = 4.0f;
	style.GrabRounding = 3.0f;
	style.PopupRounding = 4.0f;
	style.TabRounding = 2.0f;
	style.WindowMenuButtonPosition = ImGuiDir_Right;
	style.ScrollbarSize = 10.0f;
	style.GrabMinSize = 10.0f;
	style.DockingSeparatorSize = 1.0f;
	style.SeparatorTextBorderSize = 2.0f;
	style.FramePadding.x = 20.0f;
	style.DockTabPadding.x = 0.0f;
	style.IndentSpacing = 8.0f;
}

void UIManager::createDescriptorPool() {
	VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
										 {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
										 {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
										 {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
										 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
										 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
										 {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};
	VkDescriptorPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	pool_info.maxSets = 0;
	for (VkDescriptorPoolSize& pool_size : pool_sizes)
		pool_info.maxSets += pool_size.descriptorCount;
	pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
	pool_info.pPoolSizes = pool_sizes;
	err = vkCreateDescriptorPool(vulkanDevice->getDevice(), &pool_info, nullptr, &imguiDescriptorPool);
	checkVkResult(err);
}

void UIManager::createRenderPass() {
	VkAttachmentDescription attachment{};
	attachment.format = vulkanDevice->getSwapChainImageFormat();
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment{};
	color_attachment.attachment = 0;
	color_attachment.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment;

	VkSubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	VkRenderPassCreateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	info.attachmentCount = 1;
	info.pAttachments = &attachment;
	info.subpassCount = 1;
	info.pSubpasses = &subpass;
	info.dependencyCount = 1;
	info.pDependencies = &dependency;

	if (vkCreateRenderPass(vulkanDevice->getDevice(), &info, nullptr, &imGuiRenderPass) != VK_SUCCESS) {
		throw std::runtime_error("Could not create Dear ImGui's render pass");
	}
}

void UIManager::createCommandPool() {
	VkCommandPoolCreateInfo commandPoolCreateInfo{};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.queueFamilyIndex = vulkanDevice->getGraphicsQueueFamily();
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(vulkanDevice->getDevice(), &commandPoolCreateInfo, nullptr, &imGuiCommandPool) !=
		VK_SUCCESS) {
		throw std::runtime_error("Could not create graphics command pool");
	}
}

void UIManager::createCommandBuffers(uint32_t imageCount) {
	imGuiCommandBuffers.resize(imageCount);

	VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandPool = imGuiCommandPool;
	commandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>(imGuiCommandBuffers.size());
	vkAllocateCommandBuffers(vulkanDevice->getDevice(), &commandBufferAllocateInfo, imGuiCommandBuffers.data());
}

void UIManager::createFramebuffers(const std::vector<VkImageView>& swapchainImageViews) {
	imGuiFramebuffers.resize(swapchainImageViews.size());

	VkImageView attachment[1];
	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.renderPass = imGuiRenderPass;
	frameBufferCreateInfo.attachmentCount = 1;
	frameBufferCreateInfo.pAttachments = attachment;
	frameBufferCreateInfo.width = vulkanDevice->getSwapChainExtent().width;
	frameBufferCreateInfo.height = vulkanDevice->getSwapChainExtent().height;
	frameBufferCreateInfo.layers = 1;
	for (uint32_t i = 0; i < swapchainImageViews.size(); i++) {
		attachment[0] = swapchainImageViews[i];
		err = vkCreateFramebuffer(vulkanDevice->getDevice(), &frameBufferCreateInfo, nullptr, &imGuiFramebuffers[i]);
		checkVkResult(err);
	}
}

void UIManager::cleanupFramebuffers() {
	for (size_t i = 0; i < imGuiFramebuffers.size(); i++) {
		vkDestroyFramebuffer(vulkanDevice->getDevice(), imGuiFramebuffers[i], nullptr);
	}
	imGuiFramebuffers.clear();
}
