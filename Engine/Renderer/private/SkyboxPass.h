#pragma once

#include <core/Types.h>
#include <rhi/RHIBindGroup.h>
#include <rhi/RHICommandBuffer.h>
#include <rhi/RHIDevice.h>
#include <rhi/RHIPipeline.h>
#include <rhi/RHIRenderPass.h>

namespace VKE::Renderer {

    // Fills whatever the scene did not cover with the environment cubemap.
    class SkyboxPass {
    public:
        SkyboxPass(RHI::RHIDevice& device, RHI::RHIRenderPass& scenePass, RHI::RHIBindGroupLayout& frameLayout,
                   RHI::RHIBindGroupLayout& environmentLayout);

        void Record(RHI::RHICommandBuffer& cmd, RHI::RHIBindGroup& frameGroup, RHI::RHIBindGroup& environmentGroup);

    private:
        Iunique<RHI::RHIPipeline> m_Pipeline;
    };

} // namespace VKE::Renderer
