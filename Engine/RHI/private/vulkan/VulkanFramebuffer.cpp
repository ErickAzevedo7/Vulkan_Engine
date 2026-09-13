#include "VulkanFramebuffer.h"
#include "VulkanDevice.h"
#include "VulkanRenderPass.h"
#include "VulkanUtils.h"

namespace VKE::RHI {

    VulkanFramebuffer::VulkanFramebuffer(VulkanDevice& device, VulkanRenderPass& renderPass, const FramebufferDesc& desc)
        : m_Device(device), m_Width(desc.Width), m_Height(desc.Height) {
        std::vector<VkImageView> views;
        views.reserve(desc.AttachmentViews.size());
        for (void* view : desc.AttachmentViews) {
            views.push_back(static_cast<VkImageView>(view));
        }

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass.GetHandle();
        framebufferInfo.attachmentCount = static_cast<u32>(views.size());
        framebufferInfo.pAttachments = views.data();
        framebufferInfo.width = desc.Width;
        framebufferInfo.height = desc.Height;
        framebufferInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(m_Device.GetDevice(), &framebufferInfo, nullptr, &m_Framebuffer));
    }

    VulkanFramebuffer::~VulkanFramebuffer() {
        if (m_Framebuffer) {
            vkDestroyFramebuffer(m_Device.GetDevice(), m_Framebuffer, nullptr);
        }
    }

    Iunique<RHIFramebuffer> RHIFramebuffer::Create(RHIDevice& device, RHIRenderPass& renderPass, const FramebufferDesc& desc) {
        return MakeIunique<VulkanFramebuffer>(static_cast<VulkanDevice&>(device), static_cast<VulkanRenderPass&>(renderPass),
                                               desc);
    }

} // namespace VKE::RHI
