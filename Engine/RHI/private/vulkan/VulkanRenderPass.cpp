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

        bool sampledAfter = desc.DepthAttachment.SampledAfter;

        for (const auto& color : desc.ColorAttachments) {
            VkImageLayout finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            if (color.PresentAfter) {
                finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            } else if (color.SampledAfter) {
                finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                sampledAfter = true;
            }
            addAttachment(color, finalLayout);

            VkAttachmentReference ref{};
            ref.attachment = static_cast<u32>(attachments.size() - 1);
            ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorRefs.push_back(ref);
        }

        if (hasDepth) {
            // Same layout bind groups expect for sampled textures.
            addAttachment(desc.DepthAttachment, desc.DepthAttachment.SampledAfter
                                                    ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                                    : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
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
            dependency.srcStageMask |=
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependency.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            dependency.dstStageMask |=
                VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        if (sampledAfter) {
            // A previous frame may still be sampling the attachment this pass overwrites.
            dependency.srcStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<u32>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        std::vector<VkSubpassDependency> dependencies{dependency};
        if (sampledAfter) {
            // Makes what the pass wrote visible to the shaders that sample it next.
            VkSubpassDependency readDependency{};
            readDependency.srcSubpass = 0;
            readDependency.dstSubpass = VK_SUBPASS_EXTERNAL;
            readDependency.srcStageMask =
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            readDependency.srcAccessMask =
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            readDependency.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            readDependency.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dependencies.push_back(readDependency);
        }

        renderPassInfo.dependencyCount = static_cast<u32>(dependencies.size());
        renderPassInfo.pDependencies = dependencies.data();

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
