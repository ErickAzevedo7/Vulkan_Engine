#pragma once

#include <core/Types.h>
#include <rhi/RHITypes.h>

namespace VKE::RHI {

    class RHIDevice;
    class RHIRenderPass;
    class RHIFramebuffer;
    class RHIPipeline;
    class RHIBuffer;

    // Records GPU commands for a single frame. One command buffer per
    // frame-in-flight, reused (Begin/End) rather than reallocated.
    // Per-frame synchronization is owned by RHISwapchain, not by the
    // command buffer itself.
    //
    // BeginRenderPass sets viewport/scissor to the full framebuffer size
    // internally; there's no separate SetViewport call yet.
    class RHICommandBuffer {
    public:
        virtual ~RHICommandBuffer() = default;

        static Iunique<RHICommandBuffer> Create(RHIDevice& device);

        virtual void Begin() = 0;
        virtual void End() = 0;

        virtual void BeginRenderPass(RHIRenderPass& renderPass, RHIFramebuffer& framebuffer, const ClearValue& clear) = 0;
        virtual void EndRenderPass() = 0;

        virtual void BindPipeline(RHIPipeline& pipeline) = 0;
        virtual void BindVertexBuffer(RHIBuffer& buffer) = 0;

        virtual void Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex, u32 firstInstance) = 0;
    };

} // namespace VKE::RHI
