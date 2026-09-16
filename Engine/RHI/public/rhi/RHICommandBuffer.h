#pragma once

#include <core/Types.h>
#include <rhi/RHITypes.h>

namespace VKE::RHI {

    class RHIDevice;
    class RHIRenderPass;
    class RHIFramebuffer;
    class RHIPipeline;
    class RHIBuffer;
    class RHIBindGroup;

    // Records GPU commands. Keep one per frame in flight and reuse it.
    class RHICommandBuffer {
    public:
        virtual ~RHICommandBuffer() = default;

        static Iunique<RHICommandBuffer> Create(RHIDevice& device);

        virtual void Begin() = 0;
        virtual void End() = 0;

        // Also sets the viewport and scissor to cover the whole framebuffer.
        virtual void BeginRenderPass(RHIRenderPass& renderPass, RHIFramebuffer& framebuffer, const ClearValue& clear) = 0;
        virtual void EndRenderPass() = 0;

        virtual void SetViewport(const Viewport& viewport) = 0;
        virtual void SetScissor(const ScissorRect& rect) = 0;

        virtual void BindPipeline(RHIPipeline& pipeline) = 0;
        virtual void BindVertexBuffer(RHIBuffer& buffer, size_t offset = 0) = 0;
        virtual void BindIndexBuffer(RHIBuffer& buffer, IndexType type, size_t offset = 0) = 0;

        // Binds `group` at `index` for the bound pipeline. Pass one offset per
        // UniformBufferDynamic binding, in binding order.
        virtual void BindBindGroup(u32 index, RHIBindGroup& group, const u32* dynamicOffsets = nullptr,
                                   u32 dynamicOffsetCount = 0) = 0;

        // Sets push constant data for the bound pipeline.
        virtual void PushConstants(ShaderStageFlags stages, u32 offset, u32 size, const void* data) = 0;

        virtual void Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) = 0;
        virtual void DrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex, i32 vertexOffset,
                                 u32 firstInstance) = 0;
    };

} // namespace VKE::RHI
