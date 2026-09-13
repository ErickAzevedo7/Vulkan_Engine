#include "VulkanCommandBuffer.h"
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
        // The command pool was created with VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
        // (VulkanDevice::CreateCommandPool), so re-beginning an already-recorded
        // buffer implicitly resets it — no explicit vkResetCommandBuffer needed.
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VK_CHECK(vkBeginCommandBuffer(m_CommandBuffer, &beginInfo));
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

        VkViewport viewport{};
        viewport.width = static_cast<float>(vkFramebuffer.GetWidth());
        viewport.height = static_cast<float>(vkFramebuffer.GetHeight());
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(m_CommandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.extent = {vkFramebuffer.GetWidth(), vkFramebuffer.GetHeight()};
        vkCmdSetScissor(m_CommandBuffer, 0, 1, &scissor);
    }

    void VulkanCommandBuffer::EndRenderPass() {
        vkCmdEndRenderPass(m_CommandBuffer);
    }

    void VulkanCommandBuffer::BindPipeline(RHIPipeline& pipeline) {
        vkCmdBindPipeline(m_CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, static_cast<VulkanPipeline&>(pipeline).GetHandle());
    }

    void VulkanCommandBuffer::BindVertexBuffer(RHIBuffer& buffer) {
        VkBuffer buffers[] = {static_cast<VulkanBuffer&>(buffer).GetHandle()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(m_CommandBuffer, 0, 1, buffers, offsets);
    }

    void VulkanCommandBuffer::Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) {
        vkCmdDraw(m_CommandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    Iunique<RHICommandBuffer> RHICommandBuffer::Create(RHIDevice& device) {
        return MakeIunique<VulkanCommandBuffer>(static_cast<VulkanDevice&>(device));
    }

} // namespace VKE::RHI
