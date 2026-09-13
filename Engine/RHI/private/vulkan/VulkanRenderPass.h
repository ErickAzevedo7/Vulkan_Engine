#pragma once

#include <rhi/RHIRenderPass.h>
#include <vulkan/vulkan.h>

namespace VKE::RHI {

    class VulkanDevice;

    class VulkanRenderPass : public RHIRenderPass {
    public:
        VulkanRenderPass(VulkanDevice& device, const RenderPassDesc& desc);
        ~VulkanRenderPass() override;

        VkRenderPass GetHandle() const { return m_RenderPass; }

        // Needed by VulkanCommandBuffer::BeginRenderPass to build a
        // clear-value array matching this pass's attachment order/count.
        u32 GetColorAttachmentCount() const { return m_ColorAttachmentCount; }
        bool HasDepthAttachment() const { return m_HasDepthAttachment; }

    private:
        VulkanDevice& m_Device;
        VkRenderPass m_RenderPass = VK_NULL_HANDLE;
        u32 m_ColorAttachmentCount = 0;
        bool m_HasDepthAttachment = false;
    };

} // namespace VKE::RHI
