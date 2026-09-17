#pragma once

#include "VulkanAllocator.h"
#include "VulkanDescriptorAllocator.h"
#include <core/Types.h>
#include <rhi/RHIDevice.h>
#include <functional>
#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

namespace VKE::Platform {
    class IWindow;
} // namespace VKE::Platform

namespace VKE::RHI {

    // Vulkan backend for RHIDevice. Owns the instance, window surface,
    // physical and logical device, queues, command pool and memory allocator.
    class VulkanDevice : public RHIDevice {
    public:
        explicit VulkanDevice(Platform::IWindow& window);
        ~VulkanDevice() override;

        void WaitIdle() override;
        const DeviceLimits& GetLimits() const override { return m_Limits; }
        Format GetDepthFormat() const override { return m_DepthFormat; }

        // Records a one-off command buffer and blocks until the GPU has run it.
        void ImmediateSubmit(const std::function<void(VkCommandBuffer)>& record);

        // Names a Vulkan object in validation messages. No-op without validation layers.
        void SetDebugName(VkObjectType type, u64 handle, const char* name) const;

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
        const VkPhysicalDeviceFeatures& GetEnabledFeatures() const { return m_EnabledFeatures; }
        VulkanDescriptorAllocator& GetDescriptorAllocator() { return *m_DescriptorAllocator; }

        // Layout with no bindings, used for bind group indices a pipeline leaves unused.
        VkDescriptorSetLayout GetEmptySetLayout() const { return m_EmptySetLayout; }

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
        void QueryDeviceInfo();
        void CreateDescriptorResources();

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

        VkPhysicalDeviceFeatures m_EnabledFeatures{};
        DeviceLimits m_Limits;
        Format m_DepthFormat = Format::Undefined;

        Iunique<VulkanDescriptorAllocator> m_DescriptorAllocator;
        VkDescriptorSetLayout m_EmptySetLayout = VK_NULL_HANDLE;

        Iunique<VulkanAllocator> m_Allocator;
    };

} // namespace VKE::RHI
