#include "VulkanSwapchain.h"
#include "VulkanCommandBuffer.h"
#include "VulkanDevice.h"
#include "VulkanUtils.h"

#include <platform/Window.h>

#include <algorithm>
#include <limits>

namespace VKE::RHI {

    namespace {
        struct SwapChainSupportDetails {
            VkSurfaceCapabilitiesKHR Capabilities{};
            std::vector<VkSurfaceFormatKHR> Formats;
            std::vector<VkPresentModeKHR> PresentModes;
        };

        SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
            SwapChainSupportDetails details;
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.Capabilities);

            u32 formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
            details.Formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.Formats.data());

            u32 presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
            details.PresentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.PresentModes.data());

            return details;
        }

        // Prefers a UNORM format: whoever draws to the swapchain writes sRGB-encoded values, so
        // 2D drawing blends in the same space as the colors it was designed with. The renderer
        // checks GetColorFormat() and lets the hardware encode when only an _SRGB one exists.
        VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
            for (VkFormat wanted : {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM}) {
                for (const auto& format : formats) {
                    if (format.format == wanted && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                        return format;
                    }
                }
            }
            return formats[0];
        }

        VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
            for (auto mode : modes) {
                if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
            }
            return VK_PRESENT_MODE_FIFO_KHR; // always supported
        }

        VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& capabilities, Platform::IWindow& window) {
            if (capabilities.currentExtent.width != std::numeric_limits<u32>::max()) {
                return capabilities.currentExtent;
            }
            VkExtent2D extent{window.GetWidth(), window.GetHeight()};
            extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            extent.height =
                std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
            return extent;
        }
    } // namespace

    VulkanSwapchain::VulkanSwapchain(VulkanDevice& device, Platform::IWindow& window) : m_Device(device), m_Window(window) {
        CreateSwapchain();
        CreateImageViews();
        CreateSyncObjects();
    }

    VulkanSwapchain::~VulkanSwapchain() {
        CleanupSwapchain();

        VkDevice device = m_Device.GetDevice();
        for (u32 i = 0; i < kMaxFramesInFlight; ++i) {
            vkDestroySemaphore(device, m_RenderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, m_ImageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, m_InFlightFences[i], nullptr);
        }
    }

    void VulkanSwapchain::CreateSwapchain() {
        SwapChainSupportDetails support = QuerySwapChainSupport(m_Device.GetPhysicalDevice(), m_Device.GetSurface());

        VkSurfaceFormatKHR surfaceFormat = ChooseSurfaceFormat(support.Formats);
        VkPresentModeKHR presentMode = ChoosePresentMode(support.PresentModes);
        VkExtent2D extent = ChooseExtent(support.Capabilities, m_Window);

        u32 imageCount = support.Capabilities.minImageCount + 1;
        if (support.Capabilities.maxImageCount > 0 && imageCount > support.Capabilities.maxImageCount) {
            imageCount = support.Capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_Device.GetSurface();
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        // Graphics and present are frequently the same family; only opt into
        // concurrent sharing when they actually differ.
        u32 queueFamilyIndices[] = {m_Device.GetGraphicsQueueFamily(), m_Device.GetPresentQueueFamily()};
        if (queueFamilyIndices[0] == queueFamilyIndices[1]) {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        } else {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }

        createInfo.preTransform = support.Capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        VK_CHECK(vkCreateSwapchainKHR(m_Device.GetDevice(), &createInfo, nullptr, &m_Swapchain));

        vkGetSwapchainImagesKHR(m_Device.GetDevice(), m_Swapchain, &imageCount, nullptr);
        m_Images.resize(imageCount);
        vkGetSwapchainImagesKHR(m_Device.GetDevice(), m_Swapchain, &imageCount, m_Images.data());

        m_ImageFormat = surfaceFormat.format;
        m_Extent = extent;
    }

    void VulkanSwapchain::CreateImageViews() {
        m_ImageViews.resize(m_Images.size());
        for (size_t i = 0; i < m_Images.size(); ++i) {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = m_Images[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = m_ImageFormat;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.layerCount = 1;

            VK_CHECK(vkCreateImageView(m_Device.GetDevice(), &viewInfo, nullptr, &m_ImageViews[i]));
        }
    }

    void VulkanSwapchain::CreateSyncObjects() {
        m_ImageAvailableSemaphores.resize(kMaxFramesInFlight);
        m_RenderFinishedSemaphores.resize(kMaxFramesInFlight);
        m_InFlightFences.resize(kMaxFramesInFlight);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // starts signaled so the first frame doesn't stall

        for (u32 i = 0; i < kMaxFramesInFlight; ++i) {
            VK_CHECK(vkCreateSemaphore(m_Device.GetDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]));
            VK_CHECK(vkCreateSemaphore(m_Device.GetDevice(), &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]));
            VK_CHECK(vkCreateFence(m_Device.GetDevice(), &fenceInfo, nullptr, &m_InFlightFences[i]));
        }
    }

    void VulkanSwapchain::CleanupSwapchain() {
        VkDevice device = m_Device.GetDevice();
        for (auto view : m_ImageViews) {
            vkDestroyImageView(device, view, nullptr);
        }
        m_ImageViews.clear();

        if (m_Swapchain) {
            vkDestroySwapchainKHR(device, m_Swapchain, nullptr);
            m_Swapchain = VK_NULL_HANDLE;
        }
    }

    void VulkanSwapchain::Recreate() {
        // Callers skip frames while the window is minimized instead of recreating at 0x0.
        m_Device.WaitIdle();
        CleanupSwapchain();
        CreateSwapchain();
        CreateImageViews();
    }

    Format VulkanSwapchain::GetColorFormat() const {
        return FromVkFormat(m_ImageFormat);
    }

    bool VulkanSwapchain::AcquireNextImage(u32& outImageIndex) {
        vkWaitForFences(m_Device.GetDevice(), 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);

        VkResult result = vkAcquireNextImageKHR(m_Device.GetDevice(), m_Swapchain, UINT64_MAX,
                                                 m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE,
                                                 &m_CurrentImageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            return false;
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            VK_CHECK(result);
        }

        vkResetFences(m_Device.GetDevice(), 1, &m_InFlightFences[m_CurrentFrame]);
        outImageIndex = m_CurrentImageIndex;
        return true;
    }

    bool VulkanSwapchain::SubmitAndPresent(RHICommandBuffer& cmd) {
        VkCommandBuffer vkCmd = static_cast<VulkanCommandBuffer&>(cmd).GetHandle();

        VkSemaphore waitSemaphores[] = {m_ImageAvailableSemaphores[m_CurrentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphore signalSemaphores[] = {m_RenderFinishedSemaphores[m_CurrentFrame]};

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &vkCmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VK_CHECK(vkQueueSubmit(m_Device.GetGraphicsQueue(), 1, &submitInfo, m_InFlightFences[m_CurrentFrame]));

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_Swapchain;
        presentInfo.pImageIndices = &m_CurrentImageIndex;

        VkResult result = vkQueuePresentKHR(m_Device.GetPresentQueue(), &presentInfo);

        m_CurrentFrame = (m_CurrentFrame + 1) % kMaxFramesInFlight;

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            return false;
        }
        VK_CHECK(result);
        return true;
    }

    Iunique<RHISwapchain> RHISwapchain::Create(RHIDevice& device, Platform::IWindow& window) {
        return MakeIunique<VulkanSwapchain>(static_cast<VulkanDevice&>(device), window);
    }

} // namespace VKE::RHI
