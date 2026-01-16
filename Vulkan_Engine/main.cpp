#define IMGUI_DEFINE_MATH_OPERATORS
#define fontPath "C:\\Windows\\Fonts\\segoeuisl.ttf" // Windows UI font

#include "Editor/EditorCamera.h"
#include "Editor/MousePick.h"
#include "Editor/ViewPort.h"
#include "SceneRenderer.h"
#include "core/vulkancore.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "managers/SceneManager.h"
#include "postprocess/outline.h"
#include "ui/AssetBrowser.h"
#include "ui/SceneUi.h"
#include "ui/InspectorUi.h"

// GLOBAL VARIABLES
float deltaTime = 0.0f;  // Time between current frame and last frame
float lastFrame = 0.0f;  // Time of last frame

class VulkanEngine {
 public:
  void run() {
    // Initialize Vulkan
    engineCore.initWindow();
    engineCore.initVulkan();
    TextureManager::loadDefaults();
    MaterialManager::init();
    MaterialManager::loadDefault();
    SceneRenderer::init(&engineCore);
    mousePick.init(&engineCore);
    viewPort.init(&engineCore, mousePick.getMousePickExtent());
    outline.init(&engineCore, mousePick.getMousePickImageViews(),
                 viewPort.m_ViewportImageViews, mousePick.getMousePickExtent());
    editorCamera.init(&engineCore);
    SceneManager::loadDefaults();
    init();

    changeImGuizmoStyle();
    mainLoop();

    outline.cleanup();
    viewPort.cleanup();
    mousePick.cleanup();
    MaterialManager::cleanup();
    TextureManager::cleanup();

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

    vkWaitForFences(
        VulkanCore::getDevice(), 1,
        &engineCore.getInFlightFences()[VulkanCore::getCurrentFrame()], VK_TRUE,
        UINT64_MAX);
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(
        VulkanCore::getDevice(), engineCore.getSwapChain(), UINT64_MAX,
        engineCore.getImageAvailableSemaphores()[VulkanCore::getCurrentFrame()],
        VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      engineCore.recreateSwapChain();
      return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      throw std::runtime_error("failed to acquire swap chain image!");
    }

    vkResetFences(
        VulkanCore::getDevice(), 1,
        &engineCore.getInFlightFences()[VulkanCore::getCurrentFrame()]);

    vkResetCommandBuffer(
        engineCore.getCommandBuffers()[VulkanCore::getCurrentFrame()], 0);

    mousePick.recordMousePickCommandBuffer(
        mousePick.mousePickCommandBuffers[VulkanCore::getCurrentFrame()],
        imageIndex);

    viewPort.recordViewportCommandBuffer(
        viewPort.m_ViewportCommandBuffers[VulkanCore::getCurrentFrame()],
        imageIndex);

    outline.recordOutlineCommandBuffer(
        outline.outlineCommandBuffers[VulkanCore::getCurrentFrame()],
        imageIndex);

    recordImguiCommandBuffer(imGuiCommandBuffers[VulkanCore::getCurrentFrame()],
                             imageIndex);

    editorCamera.updateUniformBuffer(VulkanCore::getCurrentFrame());

    std::array<VkCommandBuffer, 4> submitCommandBuffers = {
        mousePick.mousePickCommandBuffers[VulkanCore::getCurrentFrame()],
        viewPort.m_ViewportCommandBuffers[VulkanCore::getCurrentFrame()],
        outline.outlineCommandBuffers[VulkanCore::getCurrentFrame()],
        imGuiCommandBuffers[VulkanCore::getCurrentFrame()],
    };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {
        engineCore
            .getImageAvailableSemaphores()[VulkanCore::getCurrentFrame()]};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount =
        static_cast<uint32_t>(submitCommandBuffers.size());
    submitInfo.pCommandBuffers = submitCommandBuffers.data();

    VkSemaphore signalSemaphores[] = {
        engineCore
            .getRenderFinishedSemaphores()[VulkanCore::getCurrentFrame()]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(
            VulkanCore::getGraphicsQueue(), 1, &submitInfo,
            engineCore.getInFlightFences()[VulkanCore::getCurrentFrame()]) !=
        VK_SUCCESS) {
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
    presentInfo.pResults = nullptr;  // Optional

    result = vkQueuePresentKHR(VulkanCore::getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        engineCore.getFramebufferResized()) {
      engineCore.setFramebufferResized(false);
      engineCore.recreateSwapChain();
    } else if (result != VK_SUCCESS) {
      throw std::runtime_error("failed to present swap chain image!");
    }

    engineCore.setCurrentFrame((VulkanCore::getCurrentFrame() + 1) %
                               MAX_FRAMES_IN_FLIGHT);
  }

  void mainLoop() {
    sceneTexture.resize(viewPort.m_ViewportImageViews.size());
    for (uint32_t i = 0; i < viewPort.m_ViewportImageViews.size(); i++)
      sceneTexture[i] = ImGui_ImplVulkan_AddTexture(
          engineCore.getTextureSampler(), outline.outlineColorImageViews[i],
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    while (!glfwWindowShouldClose(engineCore.getWindow())) {
      float currentFrame = glfwGetTime();
      deltaTime = currentFrame - lastFrame;
      lastFrame = currentFrame;
      glfwPollEvents();

      if (engineCore.getSwapChainRecreated()) {
        recreateRenderPasses();

        engineCore.setSwapChainRecreated(false);
      }

      ImGui_ImplVulkan_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();
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

      this->viewportExtent =
          VkExtent2D{(uint32_t)viewportSize.x, (uint32_t)viewportSize.y};

      if (viewportExtent.width != mousePick.mousePickExtent.width ||
          viewportExtent.height != mousePick.mousePickExtent.height) {
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
            SceneManager::getActiveScene()->createEntity("Empty Entity");
          }

          if (ImGui::MenuItem("Create Cube")) {
            Entity& entity =
                SceneManager::getActiveScene()->createEntity("Cube");
            MeshComponent* meshComp = new MeshComponent(&entity, "cube");
            Transform* transformComp = new Transform();
            entity.addComponent(meshComp);
            entity.addComponent(transformComp);
          }
          if (ImGui::MenuItem("Create Sphere")) {
            Entity& entity =
                SceneManager::getActiveScene()->createEntity("Sphere");
            MeshComponent* meshComp = new MeshComponent(&entity, "sphere");
            Transform* transformComp = new Transform();
            entity.addComponent(meshComp);
            entity.addComponent(transformComp);
          }
          if (ImGui::MenuItem("Create Quad")) {
            Entity& entity =
                SceneManager::getActiveScene()->createEntity("Quad");
            MeshComponent* meshComp = new MeshComponent(&entity, "quad");
            Transform* transformComp = new Transform();
            entity.addComponent(meshComp);
            entity.addComponent(transformComp);
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
  VkExtent2D viewportExtent;
  VkCommandPool imGuiCommandPool;
  std::vector<VkCommandBuffer> imGuiCommandBuffers;
  VkRenderPass imGuiRenderPass;
  std::vector<VkFramebuffer> imGuiFramebuffers;
  VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> sceneTexture;

  static void check_vk_result(VkResult err) {
    if (err == 0)
      return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
      abort();
  }

  void init() {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |=
        ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |=
        ImGuiConfigFlags_DockingEnable;  // IF using Docking Branch
    io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

    FILE* f = nullptr;
    if (fopen_s(&f, fontPath, "rb") != 0 || !f) {
      io.Fonts->AddFontDefault();
    } else {
      fclose(f);

      ImFontConfig fontCfg;
      fontCfg.SizePixels = 16.0f;

      io.Fonts->AddFontFromFileTTF(
          fontPath, fontCfg.SizePixels, &fontCfg,
          io.Fonts->GetGlyphRangesDefault() 
      );
    }


		ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.33f, 0.33f, 0.33f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.02f, 0.02f, 0.02f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.04f, 0.04f, 0.04f, 0.99f);
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
    colors[ImGuiCol_Separator] = ImVec4(0.16f, 0.16f, 0.16f, 0.50f);
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
    //colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.10f, 0.60f, 0.12f, 1.00f);
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

    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
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
    err = vkCreateDescriptorPool(VulkanCore::getDevice(), &pool_info, nullptr,
                                 &imguiDescriptorPool);
    check_vk_result(err);

    SwapChainSupportDetails swapChainSupport =
        engineCore.querySwapChainSupport(VulkanCore::getPhysicalDevice());

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
      imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkAttachmentDescription attachment{};
    attachment.format = engineCore.getSwapChainImageFormat();
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
    dependency.srcAccessMask = 0;  // or VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    if (vkCreateRenderPass(VulkanCore::getDevice(), &info, nullptr,
                           &imGuiRenderPass) != VK_SUCCESS) {
      throw std::runtime_error("Could not create Dear ImGui's render pass");
    }

    createCommandPool(&imGuiCommandPool,
                      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    imGuiCommandBuffers.resize(engineCore.getSwapChainImageViews().size());
    createCommandBuffers(imGuiCommandBuffers.data(),
                         static_cast<uint32_t>(imGuiCommandBuffers.size()),
                         imGuiCommandPool);

    createframebuffers();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(engineCore.getWindow(), true);
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = engineCore.getInstance();
    init_info.PhysicalDevice = VulkanCore::getPhysicalDevice();
    init_info.Device = VulkanCore::getDevice();
    init_info.QueueFamily = engineCore.getGraphicsQueueFamily();
    init_info.Queue = VulkanCore::getGraphicsQueue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = imguiDescriptorPool;
    init_info.PipelineInfoMain.RenderPass = imGuiRenderPass;
    init_info.PipelineInfoMain.Subpass = 0;
    init_info.MinImageCount = imageCount;
    init_info.ImageCount = imageCount;
    init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = check_vk_result;

    ImGui_ImplVulkan_Init(&init_info);
  }

  void recordImguiCommandBuffer(VkCommandBuffer commandBuffer,
                                uint32_t ImageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    err = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    check_vk_result(err);

    VkRenderPassBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = imGuiRenderPass;
    info.framebuffer = imGuiFramebuffers[ImageIndex];
    info.renderArea.extent.width = engineCore.getSwapChainExtent().width;
    info.renderArea.extent.height = engineCore.getSwapChainExtent().height;
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
    check_vk_result(err);
  }

  void createframebuffers() {
    imGuiFramebuffers.resize(engineCore.getSwapChainImageViews().size());

    VkImageView attachment[1];
    VkFramebufferCreateInfo frameBufferCreateInfo = {};
    frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    frameBufferCreateInfo.renderPass = imGuiRenderPass;
    frameBufferCreateInfo.attachmentCount = 1;
    frameBufferCreateInfo.pAttachments = attachment;
    frameBufferCreateInfo.width = engineCore.getSwapChainExtent().width;
    frameBufferCreateInfo.height = engineCore.getSwapChainExtent().height;
    frameBufferCreateInfo.layers = 1;
    for (uint32_t i = 0; i < engineCore.getSwapChainImageViews().size(); i++) {
      attachment[0] = engineCore.getSwapChainImageViews()[i];
      err = vkCreateFramebuffer(VulkanCore::getDevice(), &frameBufferCreateInfo,
                                nullptr, &imGuiFramebuffers[i]);
      check_vk_result(err);
    }
  }

  void cleanupFramebuffers() {
    for (size_t i = 0; i < imGuiFramebuffers.size(); i++) {
      vkDestroyFramebuffer(VulkanCore::getDevice(), imGuiFramebuffers[i],
                           nullptr);
    }
  }

  void createCommandPool(VkCommandPool* commandPool,
                         VkCommandPoolCreateFlags flags) {
    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolCreateInfo.queueFamilyIndex =
        engineCore.getGraphicsQueueFamily();
    commandPoolCreateInfo.flags = flags;

    if (vkCreateCommandPool(VulkanCore::getDevice(), &commandPoolCreateInfo,
                            nullptr, commandPool) != VK_SUCCESS) {
      throw std::runtime_error("Could not create graphics command pool");
    }
  }

  void createCommandBuffers(VkCommandBuffer* commandBuffer,
                            uint32_t commandBufferCount,
                            VkCommandPool& commandPool) {
    VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
    commandBufferAllocateInfo.sType =
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandPool = commandPool;
    commandBufferAllocateInfo.commandBufferCount = commandBufferCount;
    vkAllocateCommandBuffers(VulkanCore::getDevice(),
                             &commandBufferAllocateInfo, commandBuffer);
  }

  void recreateRenderPasses() {
    vkDeviceWaitIdle(VulkanCore::getDevice());
    for (uint32_t i = 0; i < viewPort.m_ViewportImageViews.size(); i++)
      ImGui_ImplVulkan_RemoveTexture(sceneTexture[i]);

    cleanupFramebuffers();
    createframebuffers();
    mousePick.recreateMousePick();
    viewPort.recreateViewport(mousePick.getMousePickExtent());
    outline.recreateOutline(mousePick.getMousePickImageViews(),
                            viewPort.m_ViewportImageViews,
                            mousePick.getMousePickExtent());

    for (uint32_t i = 0; i < viewPort.m_ViewportImageViews.size(); i++)
      sceneTexture[i] = ImGui_ImplVulkan_AddTexture(
          engineCore.getTextureSampler(), outline.outlineColorImageViews[i],
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  }

  void cleanup() {
    for (auto framebuffer : imGuiFramebuffers) {
      vkDestroyFramebuffer(VulkanCore::getDevice(), framebuffer, nullptr);
    }

    vkDestroyRenderPass(VulkanCore::getDevice(), imGuiRenderPass, nullptr);

    vkFreeCommandBuffers(VulkanCore::getDevice(), imGuiCommandPool,
                         static_cast<uint32_t>(imGuiCommandBuffers.size()),
                         imGuiCommandBuffers.data());
    vkDestroyCommandPool(VulkanCore::getDevice(), imGuiCommandPool, nullptr);

    // Resources to destroy when the program ends
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    vkDestroyDescriptorPool(VulkanCore::getDevice(), imguiDescriptorPool,
                            nullptr);
  }

  void inputProcess() { editorCamera.inputProcess(mousePick); }

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
