#pragma once

#include <rhi/RHIPipeline.h>
#include <vulkan/vulkan.h>

namespace VKE::RHI {

    class VulkanDevice;
    class VulkanRenderPass;

    class VulkanPipeline : public RHIPipeline {
    public:
        VulkanPipeline(VulkanDevice& device, VulkanRenderPass& renderPass, const PipelineDesc& desc);
        ~VulkanPipeline() override;

        VkPipeline GetHandle() const { return m_Pipeline; }
        VkPipelineLayout GetLayout() const { return m_Layout; }

    private:
        VulkanDevice& m_Device;
        VkPipelineLayout m_Layout = VK_NULL_HANDLE;
        VkPipeline m_Pipeline = VK_NULL_HANDLE;
    };

} // namespace VKE::RHI
