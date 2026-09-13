#pragma once

#include <rhi/RHICommandBuffer.h>
#include <vulkan/vulkan.h>

namespace VKE::RHI {

    class VulkanDevice;

    class VulkanCommandBuffer : public RHICommandBuffer {
    public:
        explicit VulkanCommandBuffer(VulkanDevice& device);
        ~VulkanCommandBuffer() override;

        void Begin() override;
        void End() override;

        void BeginRenderPass(RHIRenderPass& renderPass, RHIFramebuffer& framebuffer, const ClearValue& clear) override;
        void EndRenderPass() override;

        void BindPipeline(RHIPipeline& pipeline) override;
        void BindVertexBuffer(RHIBuffer& buffer) override;

        void Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) override;

        VkCommandBuffer GetHandle() const { return m_CommandBuffer; }

    private:
        VulkanDevice& m_Device;
        VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
    };

} // namespace VKE::RHI
