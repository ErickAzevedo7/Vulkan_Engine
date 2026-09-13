#pragma once

#include <rhi/RHISwapchain.h>
#include <vulkan/vulkan.h>
#include <vector>

namespace VKE::Platform {
    class IWindow;
} // namespace VKE::Platform

namespace VKE::RHI {

    class VulkanDevice;

    class VulkanSwapchain : public RHISwapchain {
    public:
        VulkanSwapchain(VulkanDevice& device, Platform::IWindow& window);
        ~VulkanSwapchain() override;

        void Recreate() override;

        u32 GetWidth() const override { return m_Extent.width; }
        u32 GetHeight() const override { return m_Extent.height; }
        Format GetColorFormat() const override;
        u32 GetImageCount() const override { return static_cast<u32>(m_Images.size()); }
        void* GetImageView(u32 index) const override { return m_ImageViews[index]; }

        u32 GetFramesInFlightCount() const override { return kMaxFramesInFlight; }
        u32 GetCurrentFrameIndex() const override { return m_CurrentFrame; }

        bool AcquireNextImage(u32& outImageIndex) override;
        bool SubmitAndPresent(RHICommandBuffer& cmd) override;

    private:
        static constexpr u32 kMaxFramesInFlight = 2;

        void CreateSwapchain();
        void CreateImageViews();
        void CreateSyncObjects();
        void CleanupSwapchain(); // destroys image views + swapchain only, not sync objects

        VulkanDevice& m_Device;
        Platform::IWindow& m_Window;

        VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
        VkFormat m_ImageFormat = VK_FORMAT_UNDEFINED;
        VkExtent2D m_Extent{};
        std::vector<VkImage> m_Images;
        std::vector<VkImageView> m_ImageViews;

        std::vector<VkSemaphore> m_ImageAvailableSemaphores;
        std::vector<VkSemaphore> m_RenderFinishedSemaphores;
        std::vector<VkFence> m_InFlightFences;
        u32 m_CurrentFrame = 0;
        u32 m_CurrentImageIndex = 0;
    };

} // namespace VKE::RHI
