#define IMGUI_DEFINE_MATH_OPERATORS
#define GLM_ENABLE_EXPERIMENTAL
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "vulkancore.h"
#include "ViewPort.h"
#include "Editor/EditorCamera.h"

//GLOBAL VARIABLES
float deltaTime = 0.0f;	// Time between current frame and last frame
float lastFrame = 0.0f; // Time of last frame   

class VulkanEngine {
public:
    void run() {
		// Initialize Vulkan
        engineCore.initWindow();
		engineCore.initVulkan();

		viewPort.init(&engineCore);
		editorCamera.init(&engineCore);
		init();
		mainLoop();
        viewPort.cleanup();
		cleanup();
		engineCore.cleanup();
    }

    void drawFrame() {
        vkWaitForFences(engineCore.getDevice(), 1, &engineCore.getInFlightFences()[engineCore.getCurrentFrame()], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(engineCore.getDevice(), engineCore.getSwapChain(), UINT64_MAX, engineCore.getImageAvailableSemaphores()[engineCore.getCurrentFrame()], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            engineCore.recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        vkResetFences(engineCore.getDevice(), 1, &engineCore.getInFlightFences()[engineCore.getCurrentFrame()]);

        vkResetCommandBuffer(engineCore.getCommandBuffers()[engineCore.getCurrentFrame()], 0);

        engineCore.recordCommandBuffer(engineCore.getCommandBuffers()[engineCore.getCurrentFrame()], imageIndex);

		viewPort.recordViewportCommandBuffer(viewPort.m_ViewportCommandBuffers[engineCore.getCurrentFrame()], imageIndex);

        recordImguiCommandBuffer(imGuiCommandBuffers[engineCore.getCurrentFrame()], imageIndex);

		editorCamera.updateUniformBuffer(engineCore.getCurrentFrame());

		std::array<VkCommandBuffer, 3> submitCommandBuffers = { engineCore.getCommandBuffers()[engineCore.getCurrentFrame()],viewPort.m_ViewportCommandBuffers[engineCore.getCurrentFrame()] ,imGuiCommandBuffers[engineCore.getCurrentFrame()] };

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = { engineCore.getImageAvailableSemaphores()[engineCore.getCurrentFrame()] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;

        submitInfo.commandBufferCount = static_cast<uint32_t>(submitCommandBuffers.size());
        submitInfo.pCommandBuffers = submitCommandBuffers.data();

        VkSemaphore signalSemaphores[] = { engineCore.getRenderFinishedSemaphores()[engineCore.getCurrentFrame()] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(engineCore.getGraphicsQueue(), 1, &submitInfo, engineCore.getInFlightFences()[engineCore.getCurrentFrame()]) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { engineCore.getSwapChain()};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr; // Optional

        result = vkQueuePresentKHR(engineCore.getPresentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || engineCore.getFramebufferResized()) {
            engineCore.setFramebufferResized(false);
            engineCore.recreateSwapChain();
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        engineCore.setCurrentFrame((engineCore.getCurrentFrame() + 1) % MAX_FRAMES_IN_FLIGHT);
    }

    void mainLoop() {
        std::vector<VkDescriptorSet> m_Dset;

        m_Dset.resize(viewPort.m_ViewportImageViews.size());
        for (uint32_t i = 0; i < viewPort.m_ViewportImageViews.size(); i++)
            m_Dset[i] = ImGui_ImplVulkan_AddTexture(engineCore.getTextureSampler(), viewPort.m_ViewportImageViews[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        while (!glfwWindowShouldClose(engineCore.getWindow())) {
            float currentFrame = glfwGetTime();
            deltaTime = currentFrame - lastFrame;
            lastFrame = currentFrame;
            glfwPollEvents();
            inputProcess();

            if (engineCore.getSwapChainRecreated())
            {
                for (uint32_t i = 0; i < viewPort.m_ViewportImageViews.size(); i++)
                    ImGui_ImplVulkan_RemoveTexture(m_Dset[i]);

				cleanupFramebuffers();
				createframebuffers();
				viewPort.cleanupFramebuffers();
				viewPort.recreateViewport();

                for (uint32_t i = 0; i < viewPort.m_ViewportImageViews.size(); i++)
                    m_Dset[i] = ImGui_ImplVulkan_AddTexture(engineCore.getTextureSampler(), viewPort.m_ViewportImageViews[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
				engineCore.setSwapChainRecreated(false);
            }

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::DockSpaceOverViewport(0,ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

            ImGui::Begin("Viewport");

            ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();

            ImGui::Image((ImTextureID) m_Dset[engineCore.getCurrentFrame()], ImVec2{viewportPanelSize.x, viewportPanelSize.y});

			ImGui::End();

			ImGui::Begin("Settings");
			ImGui::Text("Viewport Size: %dx%d", (int)viewportPanelSize.x, (int)viewportPanelSize.y);
			ImGui::End();

			ImGui::Begin("scene");
			ImGui::End();

			ImGui::Begin("Assets");
			ImGui::End();

            ImGui::Render();

            drawFrame();
        }
        vkDeviceWaitIdle(engineCore.getDevice());

        for (uint32_t i = 0; i < viewPort.m_ViewportImageViews.size(); i++)
            ImGui_ImplVulkan_RemoveTexture(m_Dset[i]);
    }
private:
	VkResult err;
    VulkanCore engineCore;
	EditorCamera editorCamera;
	ViewPort viewPort;
	VkCommandPool imGuiCommandPool;
	std::vector<VkCommandBuffer> imGuiCommandBuffers;
    VkRenderPass imGuiRenderPass;
    std::vector<VkFramebuffer> imGuiFramebuffers;
    VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;

    static void check_vk_result(VkResult err)
    {
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
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch

        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 0;
        for (VkDescriptorPoolSize& pool_size : pool_sizes)
            pool_info.maxSets += pool_size.descriptorCount;
        pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        err = vkCreateDescriptorPool(engineCore.getDevice(), &pool_info, nullptr, &imguiDescriptorPool);
        check_vk_result(err);

        SwapChainSupportDetails swapChainSupport = engineCore.querySwapChainSupport(engineCore.getPhysicalDevice());

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkAttachmentDescription attachment{};
        attachment.format = engineCore.getSwapChainImageFormat();
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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

        if (vkCreateRenderPass(engineCore.getDevice(), &info, nullptr, &imGuiRenderPass) != VK_SUCCESS) {
            throw std::runtime_error("Could not create Dear ImGui's render pass");
        }

        createCommandPool(&imGuiCommandPool, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
        imGuiCommandBuffers.resize(engineCore.getSwapChainImageViews().size());
        createCommandBuffers(imGuiCommandBuffers.data(), static_cast<uint32_t>(imGuiCommandBuffers.size()), imGuiCommandPool);

		createframebuffers();

        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForVulkan(engineCore.getWindow(), true);
        ImGui_ImplVulkan_InitInfo init_info{};
        init_info.Instance = engineCore.getInstance();
        init_info.PhysicalDevice = engineCore.getPhysicalDevice();
        init_info.Device = engineCore.getDevice();
        init_info.QueueFamily = engineCore.getGraphicsQueueFamily();
        init_info.Queue = engineCore.getGraphicsQueue();
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = imguiDescriptorPool;
        init_info.RenderPass = imGuiRenderPass;
        init_info.Subpass = 0;
        init_info.MinImageCount = imageCount;
        init_info.ImageCount = imageCount;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        init_info.Allocator = nullptr;
        init_info.CheckVkResultFn = check_vk_result;

        ImGui_ImplVulkan_Init(&init_info);
    }

    void recordImguiCommandBuffer(VkCommandBuffer commandBuffer ,uint32_t ImageIndex) {
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
        clearValue.color = { {0.0f, 0.0f, 0.0f, 1.0f} };
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
            err = vkCreateFramebuffer(engineCore.getDevice(), &frameBufferCreateInfo, nullptr, &imGuiFramebuffers[i]);
            check_vk_result(err);
        }
	}

	void cleanupFramebuffers() {
		for (size_t i = 0; i < imGuiFramebuffers.size(); i++) {
			vkDestroyFramebuffer(engineCore.getDevice(), imGuiFramebuffers[i], nullptr);
		}
	}

    void createCommandPool(VkCommandPool* commandPool, VkCommandPoolCreateFlags flags) {
        VkCommandPoolCreateInfo commandPoolCreateInfo{};
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.queueFamilyIndex = engineCore.getGraphicsQueueFamily();
        commandPoolCreateInfo.flags = flags;

        if (vkCreateCommandPool(engineCore.getDevice(), &commandPoolCreateInfo, nullptr, commandPool) != VK_SUCCESS) {
            throw std::runtime_error("Could not create graphics command pool");
        }
    }

    void createCommandBuffers(VkCommandBuffer* commandBuffer, uint32_t commandBufferCount, VkCommandPool& commandPool) {
        VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandPool = commandPool;
        commandBufferAllocateInfo.commandBufferCount = commandBufferCount;
        vkAllocateCommandBuffers(engineCore.getDevice(), &commandBufferAllocateInfo, commandBuffer);
    }

    void cleanup() {
        for (auto framebuffer : imGuiFramebuffers) {
            vkDestroyFramebuffer(engineCore.getDevice(), framebuffer, nullptr);
        }

        vkDestroyRenderPass(engineCore.getDevice(), imGuiRenderPass, nullptr);

        vkFreeCommandBuffers(engineCore.getDevice(), imGuiCommandPool, static_cast<uint32_t>(imGuiCommandBuffers.size()), imGuiCommandBuffers.data());
        vkDestroyCommandPool(engineCore.getDevice(), imGuiCommandPool, nullptr);

        // Resources to destroy when the program ends
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(engineCore.getDevice(), imguiDescriptorPool, nullptr);
    }

	void inputProcess() {
        const float cameraSpeed = 2.5f * deltaTime; // adjust accordingly
        if (glfwGetKey(engineCore.getWindow(), GLFW_KEY_W) == GLFW_PRESS)
            editorCamera.cameraPos += cameraSpeed * editorCamera.cameraFront;
        if (glfwGetKey(engineCore.getWindow(), GLFW_KEY_S) == GLFW_PRESS)
            editorCamera.cameraPos -= cameraSpeed * editorCamera.cameraFront;
        if (glfwGetKey(engineCore.getWindow(), GLFW_KEY_A) == GLFW_PRESS)
            editorCamera.cameraPos -= glm::normalize(glm::cross(editorCamera.cameraFront, editorCamera.cameraUp)) * cameraSpeed;
        if (glfwGetKey(engineCore.getWindow(), GLFW_KEY_D) == GLFW_PRESS)
            editorCamera.cameraPos += glm::normalize(glm::cross(editorCamera.cameraFront, editorCamera.cameraUp)) * cameraSpeed;
	}
};

int main() {
    VulkanEngine app;

    try {
        app.run();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

