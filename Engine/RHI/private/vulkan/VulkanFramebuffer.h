#pragma once

#include <rhi/RHIFramebuffer.h>
#include <vulkan/vulkan.h>

namespace VKE::RHI {

    class VulkanDevice;
    class VulkanRenderPass;

    class VulkanFramebuffer : public RHIFramebuffer {
    public:
        VulkanFramebuffer(VulkanDevice& device, VulkanRenderPass& renderPass, const FramebufferDesc& desc);
        ~VulkanFramebuffer() override;

        u32 GetWidth() const override { return m_Width; }
        u32 GetHeight() const override { return m_Height; }
        VkFramebuffer GetHandle() const { return m_Framebuffer; }

    private:
        VulkanDevice& m_Device;
        VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
        u32 m_Width = 0;
        u32 m_Height = 0;
    };

} // namespace VKE::RHI
