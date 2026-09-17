#pragma once

#include <rhi/RHICommandBuffer.h>
#include <vulkan/vulkan.h>

namespace VKE::RHI {

    class VulkanDevice;
    class VulkanPipeline;

    class VulkanCommandBuffer : public RHICommandBuffer {
    public:
        explicit VulkanCommandBuffer(VulkanDevice& device);
        ~VulkanCommandBuffer() override;

        void Begin() override;
        void End() override;

        void BeginRenderPass(RHIRenderPass& renderPass, RHIFramebuffer& framebuffer, const ClearValue& clear) override;
        void EndRenderPass() override;

        void SetViewport(const Viewport& viewport) override;
        void SetScissor(const ScissorRect& rect) override;

        void BindPipeline(RHIPipeline& pipeline) override;
        void BindVertexBuffer(RHIBuffer& buffer, size_t offset) override;
        void BindIndexBuffer(RHIBuffer& buffer, IndexType type, size_t offset) override;

        void BindBindGroup(u32 index, RHIBindGroup& group, const u32* dynamicOffsets, u32 dynamicOffsetCount) override;
        void PushConstants(ShaderStageFlags stages, u32 offset, u32 size, const void* data) override;

        void Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) override;
        void DrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex, i32 vertexOffset, u32 firstInstance) override;

        VkCommandBuffer GetHandle() const { return m_CommandBuffer; }

    private:
        VulkanDevice& m_Device;
        VkCommandBuffer m_CommandBuffer = VK_NULL_HANDLE;
        VulkanPipeline* m_BoundPipeline = nullptr;
    };

} // namespace VKE::RHI
