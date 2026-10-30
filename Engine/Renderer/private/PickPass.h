#pragma once

#include "GpuCache.h"

#include <core/Types.h>
#include <math/MathTypes.h>
#include <renderer/RenderTypes.h>
#include <rhi/RHIBuffer.h>
#include <rhi/RHICommandBuffer.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHIFramebuffer.h>
#include <rhi/RHIPipeline.h>
#include <rhi/RHIRenderPass.h>
#include <rhi/RHITexture.h>

namespace VKE::Renderer {

    class RenderScene;

    // Where a view's meshes are drawn with their IDs, and the buffer the picked pixel is copied
    // into for the CPU.
    struct PickTarget {
        Iunique<RHI::RHITexture> IDs;
        Iunique<RHI::RHITexture> Depth;
        Iunique<RHI::RHIFramebuffer> Framebuffer;
        Iunique<RHI::RHIBuffer> Readback;
        u32 Width = 0;
        u32 Height = 0;
    };

    // Finds the mesh under a pixel: draws every visible mesh with its instance's handle into an
    // R32G32_UINT target, only at that pixel, and copies the pixel to a buffer the CPU reads
    // once the frame is done.
    class PickPass {
    public:
        PickPass(RHI::RHIDevice& device, GpuCache& gpuCache);
        ~PickPass();

        Iunique<PickTarget> CreateTarget(u32 width, u32 height);

        // Must be called outside any render pass. (x, y) is counted from the top left.
        void Record(RHI::RHICommandBuffer& cmd, const RenderScene& scene, const Mat4& viewProjection,
                    PickTarget& target, u32 x, u32 y);

        // The instance at the pixel the last Record copied, or invalid for none. Only once the
        // GPU is done with that frame.
        static MeshInstanceID ReadResult(PickTarget& target);

    private:
        RHI::RHIDevice& m_Device;
        GpuCache& m_GpuCache;

        // Destroyed in reverse declaration order.
        Iunique<RHI::RHIRenderPass> m_RenderPass;
        Iunique<RHI::RHIPipeline> m_Pipeline;
    };

} // namespace VKE::Renderer
