#pragma once

#include "VulkanAllocator.h"
#include <core/Types.h>
#include <rhi/RHIDevice.h>
#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

namespace VKE::Platform {
    class IWindow;
} // namespace VKE::Platform

namespace VKE::RHI {

    // Vulkan backend for RHIDevice. Owns the instance, debug messenger,
    // window surface, physical/logical device, queues, a shared command
    // pool, and the VMA allocator — the same responsibilities the legacy
    // engine's VulkanCore held, minus the swapchain/render-pass/pipeline
    // state (those move to VulkanSwapchain/VulkanRenderPass/VulkanPipeline)
    // and minus the unused geometryShader feature requirement / VK_KHR_MULTIVIEW
    // device extension (neither is used by anything here).
    //
    // Owns the VkSurfaceKHR (not VulkanSwapchain) because physical device
    // selection needs it to query present support — VulkanSwapchain reuses
    // it via GetSurface().
    class VulkanDevice : public RHIDevice {
    public:
        explicit VulkanDevice(Platform::IWindow& window);
        ~VulkanDevice() override;

        void WaitIdle() override;

        // Vulkan-internal accessors: consumed by other Vulkan* backend
        // classes through a concrete VulkanDevice&, never through the
        // abstract RHIDevice interface.
        VkInstance GetInstance() const { return m_Instance; }
        VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        VkDevice GetDevice() const { return m_Device; }
        VkSurfaceKHR GetSurface() const { return m_Surface; }
        VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
        VkQueue GetPresentQueue() const { return m_PresentQueue; }
        u32 GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
        u32 GetPresentQueueFamily() const { return m_PresentQueueFamily; }
        VkCommandPool GetCommandPool() const { return m_CommandPool; }
        VmaAllocator GetAllocator() const { return m_Allocator->GetHandle(); }

    private:
        struct QueueFamilyIndices {
            std::optional<u32> Graphics;
            std::optional<u32> Present;
            bool IsComplete() const { return Graphics.has_value() && Present.has_value(); }
        };

        void CreateInstance();
        void SetupDebugMessenger();
        void CreateSurface(Platform::IWindow& window);
        void PickPhysicalDevice();
        void CreateLogicalDevice();
        void CreateCommandPool();

        int RateDeviceSuitability(VkPhysicalDevice device) const;
        QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) const;
        bool CheckDeviceExtensionSupport(VkPhysicalDevice device) const;
        std::vector<const char*> GetRequiredInstanceExtensions() const;
        bool CheckValidationLayerSupport() const;

        VkInstance m_Instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_Device = VK_NULL_HANDLE;
        VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
        VkQueue m_PresentQueue = VK_NULL_HANDLE;
        u32 m_GraphicsQueueFamily = 0;
        u32 m_PresentQueueFamily = 0;
        VkCommandPool m_CommandPool = VK_NULL_HANDLE;

        Iunique<VulkanAllocator> m_Allocator;
    };

} // namespace VKE::RHI
