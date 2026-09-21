#include "VulkanCommandBuffer.h"
#include "VulkanBindGroup.h"
#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "VulkanFramebuffer.h"
#include "VulkanPipeline.h"
#include "VulkanRenderPass.h"
#include "VulkanUtils.h"

namespace VKE::RHI {

    VulkanCommandBuffer::VulkanCommandBuffer(VulkanDevice& device) : m_Device(device) {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_Device.GetCommandPool();
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VK_CHECK(vkAllocateCommandBuffers(m_Device.GetDevice(), &allocInfo, &m_CommandBuffer));
    }

    VulkanCommandBuffer::~VulkanCommandBuffer() {
        if (m_CommandBuffer) {
            vkFreeCommandBuffers(m_Device.GetDevice(), m_Device.GetCommandPool(), 1, &m_CommandBuffer);
        }
    }

    void VulkanCommandBuffer::Begin() {
        Begin(0);
    }

    void VulkanCommandBuffer::Begin(VkCommandBufferUsageFlags usage) {
        // The command pool allows per-buffer reset, so beginning again resets the buffer.
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = usage;
        VK_CHECK(vkBeginCommandBuffer(m_CommandBuffer, &beginInfo));
        m_BoundPipeline = nullptr;
    }

    void VulkanCommandBuffer::End() {
        VK_CHECK(vkEndCommandBuffer(m_CommandBuffer));
    }

    void VulkanCommandBuffer::BeginRenderPass(RHIRenderPass& renderPass, RHIFramebuffer& framebuffer, const ClearValue& clear) {
        auto& vkRenderPass = static_cast<VulkanRenderPass&>(renderPass);
        auto& vkFramebuffer = static_cast<VulkanFramebuffer&>(framebuffer);

        std::vector<VkClearValue> clearValues;
        clearValues.reserve(vkRenderPass.GetColorAttachmentCount() + (vkRenderPass.HasDepthAttachment() ? 1 : 0));
        for (u32 i = 0; i < vkRenderPass.GetColorAttachmentCount(); ++i) {
            VkClearValue value{};
            value.color = {{clear.R, clear.G, clear.B, clear.A}};
            clearValues.push_back(value);
        }
        if (vkRenderPass.HasDepthAttachment()) {
            VkClearValue value{};
            value.depthStencil = {clear.Depth, 0};
            clearValues.push_back(value);
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = vkRenderPass.GetHandle();
        renderPassInfo.framebuffer = vkFramebuffer.GetHandle();
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = {vkFramebuffer.GetWidth(), vkFramebuffer.GetHeight()};
        renderPassInfo.clearValueCount = static_cast<u32>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(m_CommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        Viewport viewport;
        viewport.Width = static_cast<f32>(vkFramebuffer.GetWidth());
        viewport.Height = static_cast<f32>(vkFramebuffer.GetHeight());
        SetViewport(viewport);

        ScissorRect scissor;
        scissor.Width = vkFramebuffer.GetWidth();
        scissor.Height = vkFramebuffer.GetHeight();
        SetScissor(scissor);
    }

    void VulkanCommandBuffer::EndRenderPass() {
        vkCmdEndRenderPass(m_CommandBuffer);
    }

    void VulkanCommandBuffer::SetViewport(const Viewport& viewport) {
        // Negative height flips Vulkan's Y-down clip space to Y-up.
        VkViewport vkViewport{};
        vkViewport.x = viewport.X;
        vkViewport.y = viewport.Y + viewport.Height;
        vkViewport.width = viewport.Width;
        vkViewport.height = -viewport.Height;
        vkViewport.minDepth = viewport.MinDepth;
        vkViewport.maxDepth = viewport.MaxDepth;
        vkCmdSetViewport(m_CommandBuffer, 0, 1, &vkViewport);
    }

    void VulkanCommandBuffer::SetScissor(const ScissorRect& rect) {
        VkRect2D scissor{};
        scissor.offset = {rect.X, rect.Y};
        scissor.extent = {rect.Width, rect.Height};
        vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);
    }

    void VulkanCommandBuffer::BindPipeline(RHIPipeline& pipeline) {
        m_BoundPipeline = &static_cast<VulkanPipeline&>(pipeline);
        vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_BoundPipeline->GetHandle());
    }

    void VulkanCommandBuffer::BindVertexBuffer(RHIBuffer& buffer, size_t offset) {
        VkBuffer handle = static_cast<VulkanBuffer&>(buffer).GetHandle();
        VkDeviceSize vkOffset = offset;
        vkCmdBindVertexBuffers(m_CommandBuffer, 0, 1, &handle, &vkOffset);
    }

    void VulkanCommandBuffer::BindIndexBuffer(RHIBuffer& buffer, IndexType type, size_t offset) {
        vkCmdBindIndexBuffer(m_CommandBuffer, static_cast<VulkanBuffer&>(buffer).GetHandle(), offset, ToVkIndexType(type));
    }

    void VulkanCommandBuffer::BindBindGroup(u32 index, RHIBindGroup& group, const u32* dynamicOffsets,
                                            u32 dynamicOffsetCount) {
        VKE_CORE_ASSERT(m_BoundPipeline, "BindBindGroup requires a bound pipeline");
        auto& vkGroup = static_cast<VulkanBindGroup&>(group);
        VKE_CORE_ASSERT(dynamicOffsetCount == vkGroup.GetDynamicOffsetCount(), "Wrong number of dynamic offsets");

        VkDescriptorSet set = vkGroup.GetHandle();
        vkCmdBindDescriptorSets(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_BoundPipeline->GetLayout(), index, 1,
                                &set, dynamicOffsetCount, dynamicOffsets);
    }

    void VulkanCommandBuffer::PushConstants(ShaderStageFlags stages, u32 offset, u32 size, const void* data) {
        VKE_CORE_ASSERT(m_BoundPipeline, "PushConstants requires a bound pipeline");
        vkCmdPushConstants(m_CommandBuffer, m_BoundPipeline->GetLayout(), ToVkShaderStages(stages), offset, size, data);
    }

    void VulkanCommandBuffer::Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) {
        vkCmdDraw(m_CommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    void VulkanCommandBuffer::DrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex, i32 vertexOffset,
                                          u32 firstInstance) {
        vkCmdDrawIndexed(m_CommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }

    Iunique<RHICommandBuffer> RHICommandBuffer::Create(RHIDevice& device) {
        return MakeIunique<VulkanCommandBuffer>(static_cast<VulkanDevice&>(device));
    }

} // namespace VKE::RHI
