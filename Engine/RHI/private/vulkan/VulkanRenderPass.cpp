#include "VulkanRenderPass.h"
#include "VulkanDevice.h"
#include "VulkanUtils.h"

namespace VKE::RHI {

    VulkanRenderPass::VulkanRenderPass(VulkanDevice& device, const RenderPassDesc& desc)
        : m_Device(device), m_ColorAttachmentCount(static_cast<u32>(desc.ColorAttachments.size())),
          m_HasDepthAttachment(desc.HasDepthAttachment) {
        std::vector<VkAttachmentDescription> attachments;
        std::vector<VkAttachmentReference> colorRefs;
        VkAttachmentReference depthRef{};
        bool hasDepth = desc.HasDepthAttachment;

        auto addAttachment = [&](const AttachmentDesc& a, VkImageLayout finalLayout) {
            VkAttachmentDescription attachment{};
            attachment.format = ToVkFormat(a.Format);
            attachment.samples = VK_SAMPLE_COUNT_1_BIT; // No multisampling — nothing here targets an MSAA image.
            attachment.loadOp = a.Clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment.finalLayout = finalLayout;
            attachments.push_back(attachment);
        };

        for (const auto& color : desc.ColorAttachments) {
            VkImageLayout finalLayout =
                color.PresentAfter ? VK_IMAGE_LAYOUT_PRESENT_SRC_KHR : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            addAttachment(color, finalLayout);

            VkAttachmentReference ref{};
            ref.attachment = static_cast<u32>(attachments.size() - 1);
            ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorRefs.push_back(ref);
        }

        if (hasDepth) {
            addAttachment(desc.DepthAttachment, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            depthRef.attachment = static_cast<u32>(attachments.size() - 1);
            depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = static_cast<u32>(colorRefs.size());
        subpass.pColorAttachments = colorRefs.data();
        subpass.pDepthStencilAttachment = hasDepth ? &depthRef : nullptr;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        if (hasDepth) {
            dependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<u32>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        VK_CHECK(vkCreateRenderPass(m_Device.GetDevice(), &renderPassInfo, nullptr, &m_RenderPass));
    }

    VulkanRenderPass::~VulkanRenderPass() {
        if (m_RenderPass) {
            vkDestroyRenderPass(m_Device.GetDevice(), m_RenderPass, nullptr);
        }
    }

    Iunique<RHIRenderPass> RHIRenderPass::Create(RHIDevice& device, const RenderPassDesc& desc) {
        return MakeIunique<VulkanRenderPass>(static_cast<VulkanDevice&>(device), desc);
    }

} // namespace VKE::RHI
